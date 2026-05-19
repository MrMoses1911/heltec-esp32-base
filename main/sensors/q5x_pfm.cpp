#include "sensors/q5x_pfm.h"
#include "Arduino.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_attr.h"

static constexpr float kFreqRange = Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz;
static constexpr float kDistRange = Q5X_PFM::kMaxDistMm - Q5X_PFM::kMinDistMm;
static constexpr float kSlope = kDistRange / kFreqRange;
static constexpr uint32_t kMinPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMaxFreqHz);
static constexpr uint32_t kMaxPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMinFreqHz);

// ── ISR ─────────────────────────────────────────────────────────
// Uses ANYEDGE. On RISING: computes period between consecutive
// rising edges. On FALLING: measures pulse width to validate
// the waveform is healthy (rejects glitches/noise).
static volatile uint32_t g_isrLastRisingUs = 0;
static volatile uint32_t g_isrLastPwUs = 0;

void IRAM_ATTR Q5X_PFM::isrHandler(void *arg) {
  auto *self = static_cast<Q5X_PFM *>(arg);
  uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
  bool isHigh = gpio_get_level(static_cast<gpio_num_t>(self->m_gpio));

  if (isHigh) {
    // ── Rising edge: period between consecutive rising edges ──
    uint32_t periodUs = nowUs - g_isrLastRisingUs;
    g_isrLastRisingUs = nowUs;

    // Validate period
    if (periodUs < kMinPeriodUs || periodUs > kMaxPeriodUs) {
      self->m_isrRejected = self->m_isrRejected + 1u;
      return;
    }

    // Validate pulse width of the preceding pulse:
    // must be 15%–85% of this period (rejects odd duty cycles from noise)
    uint32_t minPw = periodUs / 7;   // ~14%
    uint32_t maxPw = periodUs - minPw;  // ~86%
    if (g_isrLastPwUs < minPw || g_isrLastPwUs > maxPw) {
      self->m_isrRejected = self->m_isrRejected + 1u;
      return;
    }

    // Store valid period in circular buffer
    uint8_t idx = self->m_isrIdx;
    self->m_isrBuf[idx] = periodUs;
    self->m_isrIdx = (idx + 1) % kBufSize;
    uint8_t c = self->m_isrCount;
    self->m_isrCount = (c < kBufSize) ? static_cast<uint8_t>(c + 1) : c;
    self->m_isrNewData = true;
    self->m_isrLastValidUs = nowUs;

  } else {
    // ── Falling edge: measure pulse width ──
    g_isrLastPwUs = nowUs - g_isrLastRisingUs;
  }
}

// ── update() ────────────────────────────────────────────────────
// Converts raw periods into distance using:
//  1. Median of buffer (resistant to single outliers)
//  2. MAD-based outlier rejection on the buffer itself
//  3. EMA with clamp to smooth the final reading

void Q5X_PFM::init(int gpio) {
  m_gpio = gpio;
  m_initialized = false;
  m_frequencyHz = 0.0f;
  m_distanceMm = 0.0f;
  m_emaDistanceMm = 0.0f;
  m_status = kNotInitialized;
  m_readPeriodUs = 0;
  m_isrIdx = 0;
  m_isrCount = 0;
  m_isrNewData = false;
  m_isrLastValidUs = 0;
  m_isrRejected = 0;
  for (uint8_t i = 0; i < kBufSize; i++) m_isrBuf[i] = 0;

  gpio_config_t cfg = {};
  cfg.pin_bit_mask = 1ULL << gpio;
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&cfg);

  // Init ISR shared state BEFORE enabling the ISR
  g_isrLastRisingUs = static_cast<uint32_t>(esp_timer_get_time());
  g_isrLastPwUs = 2000;

  gpio_install_isr_service(0);
  gpio_isr_handler_add(static_cast<gpio_num_t>(gpio), isrHandler, this);

  m_isrLastValidUs = g_isrLastRisingUs;
  m_initialized = true;
  m_status = kOk;
}

void Q5X_PFM::update() {
  if (!m_initialized) { m_status = kNotInitialized; return; }

  uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
  if ((nowUs - m_isrLastValidUs) > kNoSignalGapUs) {
    m_status = kNoSignal;
    return;
  }

  if (!m_isrNewData) return;
  m_isrNewData = false;

  uint8_t n = m_isrCount;
  if (n < 4) return;  // need at least 4 samples for reliable stats

  // 1. Copy the ring buffer to a local sorted array
  uint32_t sorted[kBufSize];
  for (uint8_t i = 0; i < n; i++) sorted[i] = m_isrBuf[i];
  for (uint8_t i = 0; i < n - 1; i++)
    for (uint8_t j = 0; j < n - 1 - i; j++)
      if (sorted[j] > sorted[j + 1]) {
        uint32_t t = sorted[j]; sorted[j] = sorted[j + 1]; sorted[j + 1] = t;
      }

  // 2. Median
  uint32_t med = sorted[n / 2];
  if (med == 0) return;

  // 3. MAD (Median Absolute Deviation) — robust dispersion estimate
  uint32_t deviations[kBufSize];
  for (uint8_t i = 0; i < n; i++) {
    uint32_t d = (sorted[i] > med) ? (sorted[i] - med) : (med - sorted[i]);
    deviations[i] = d;
  }
  for (uint8_t i = 0; i < n - 1; i++)
    for (uint8_t j = 0; j < n - 1 - i; j++)
      if (deviations[j] > deviations[j + 1]) {
        uint32_t t = deviations[j]; deviations[j] = deviations[j + 1]; deviations[j + 1] = t;
      }
  uint32_t mad = deviations[n / 2];

  // 4. Reject the new sample if it deviates too far from median
  //    (we check the most recently inserted sample)
  uint8_t lastIdx = (m_isrIdx == 0) ? (kBufSize - 1) : (m_isrIdx - 1);
  uint32_t newest = m_isrBuf[lastIdx];
  uint32_t newDev = (newest > med) ? (newest - med) : (med - newest);
  uint32_t rejectLimit = mad * static_cast<uint32_t>(kMadReject);
  if (newDev > rejectLimit && mad > 0) {
    // Replace the outlier with last good median and don't update
    m_isrBuf[lastIdx] = med;
    return;
  }

  // 5. Compute distance from median period
  m_readPeriodUs = med;
  m_frequencyHz = 1000000.0f / static_cast<float>(med);

  if (m_frequencyHz < kMinFreqHz || m_frequencyHz > kMaxFreqHz) {
    m_status = (m_frequencyHz < 80.0f) ? kNoSignal : kInvalid;
    return;
  }

  float rawDist = kMinDistMm + (m_frequencyHz - kMinFreqHz) * kSlope;

  // 6. EMA with adaptive clamping
  if (m_emaDistanceMm < 0.1f) {
    m_emaDistanceMm = rawDist;
    m_distanceMm = rawDist;
  } else {
    float prev = m_emaDistanceMm;
    m_emaDistanceMm = (prev * (1.0f - kEmaAlpha)) + (rawDist * kEmaAlpha);

    // Strong clamp: never move more than 50mm per update
    float delta = m_emaDistanceMm - m_distanceMm;
    if (delta > 50.0f)        m_emaDistanceMm = m_distanceMm + 50.0f;
    else if (delta < -50.0f)  m_emaDistanceMm = m_distanceMm - 50.0f;

    m_distanceMm = m_emaDistanceMm;
  }

  if (m_distanceMm < kMinDistMm) m_distanceMm = kMinDistMm;
  if (m_distanceMm > kMaxDistMm) m_distanceMm = kMaxDistMm;
  m_status = kOk;
}

float Q5X_PFM::getFrequencyHz()    const { return m_frequencyHz; }
float Q5X_PFM::getDistanceMm()     const { return m_distanceMm; }
uint32_t Q5X_PFM::getPeriodUs()    const { return m_readPeriodUs; }
uint32_t Q5X_PFM::getRejected()    const { return m_isrRejected; }
Q5X_PFM::Status Q5X_PFM::getStatus() const { return m_status; }
