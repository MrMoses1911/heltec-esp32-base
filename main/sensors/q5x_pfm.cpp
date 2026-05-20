#include "sensors/q5x_pfm.h"
#include "Arduino.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "shared/build_features.h"

static constexpr float kFreqRange = Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz;
static constexpr float kDistRange = Q5X_PFM::kMaxDistMm - Q5X_PFM::kMinDistMm;
static constexpr float kSlope = kDistRange / kFreqRange;
static constexpr uint32_t kMinPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMaxFreqHz);
static constexpr uint32_t kMaxPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMinFreqHz);

void IRAM_ATTR Q5X_PFM::isrHandler(void *arg) {
  auto *self = static_cast<Q5X_PFM *>(arg);
  uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
  uint32_t dtUs = nowUs - self->m_isrLastEdgeUs;

  if (dtUs >= kMinPeriodUs && dtUs <= kMaxPeriodUs) {
    self->m_isrLastEdgeUs = nowUs;
    self->m_isrLastValidUs = nowUs;
    self->m_isrValidPulses = self->m_isrValidPulses + 1u;

    uint8_t idx = self->m_isrIdx;
    self->m_isrBuf[idx] = dtUs;
    self->m_isrIdx = (idx + 1) % kBufSize;
    uint8_t c = self->m_isrCount;
    self->m_isrCount = (c < kBufSize) ? static_cast<uint8_t>(c + 1) : c;
    self->m_isrNewData = true;
  } else if (dtUs < kMinPeriodUs) {
    self->m_isrRejected = self->m_isrRejected + 1u;
  } else {
    self->m_isrLastEdgeUs = nowUs;
  }
}

void Q5X_PFM::init(int gpio) {
  m_gpio = gpio;
  m_initialized = false;
  m_frequencyHz = 0.0f;
  m_distanceMm = 0.0f;
  m_emaDistanceMm = 0.0f;
  m_status = kNotInitialized;
  m_readPeriodUs = 0;
  m_isrLastEdgeUs = 0;
  m_isrIdx = 0;
  m_isrCount = 0;
  m_isrNewData = false;
  m_isrLastValidUs = 0;
  m_isrRejected = 0;
  m_isrValidPulses = 0;
  for (uint8_t i = 0; i < kBufSize; i++) m_isrBuf[i] = 0;

  gpio_config_t cfg = {};
  cfg.pin_bit_mask = 1ULL << gpio;
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_POSEDGE;
  gpio_config(&cfg);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(static_cast<gpio_num_t>(gpio), isrHandler, this);

  m_isrLastEdgeUs = static_cast<uint32_t>(esp_timer_get_time());
  m_isrLastValidUs = m_isrLastEdgeUs;
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

#if PULSE_FEATURE_SENSOR_FILTER
  uint8_t n = m_isrCount;
  if (n < 4) return;

  // --- copy + sort ---
  uint32_t sorted[kBufSize];
  for (uint8_t i = 0; i < n; i++) sorted[i] = m_isrBuf[i];
  for (uint8_t i = 0; i < n - 1; i++)
    for (uint8_t j = 0; j < n - 1 - i; j++)
      if (sorted[j] > sorted[j + 1]) {
        uint32_t t = sorted[j]; sorted[j] = sorted[j + 1]; sorted[j + 1] = t;
      }

  // --- median ---
  uint32_t med = sorted[n / 2];
  if (med == 0) return;

  // --- MAD (median absolute deviation) ---
  uint32_t devs[kBufSize];
  for (uint8_t i = 0; i < n; i++)
    devs[i] = (sorted[i] > med) ? (sorted[i] - med) : (med - sorted[i]);
  for (uint8_t i = 0; i < n - 1; i++)
    for (uint8_t j = 0; j < n - 1 - i; j++)
      if (devs[j] > devs[j + 1]) {
        uint32_t t = devs[j]; devs[j] = devs[j + 1]; devs[j + 1] = t;
      }
  uint32_t mad = devs[n / 2];

  // --- reject extreme outlier ---
  uint8_t lastIdx = (m_isrIdx == 0) ? (kBufSize - 1) : (m_isrIdx - 1);
  uint32_t newest = m_isrBuf[lastIdx];
  uint32_t newestDev = (newest > med) ? (newest - med) : (med - newest);
  if (newestDev > static_cast<uint32_t>(kMadReject) * mad && mad > 0) {
    m_isrBuf[lastIdx] = med;  // replace spike
    return;
  }

  m_readPeriodUs = med;
  m_frequencyHz = 1000000.0f / static_cast<float>(med);
#else
  // Raw mode: latest valid period → Hz → mm, no median or filtering
  uint8_t lastIdx = (m_isrIdx == 0) ? (kBufSize - 1) : (m_isrIdx - 1);
  uint32_t latest = m_isrBuf[lastIdx];
  if (latest == 0) return;

  m_readPeriodUs = latest;
  m_frequencyHz = 1000000.0f / static_cast<float>(latest);
#endif
  if (m_frequencyHz < kMinFreqHz || m_frequencyHz > kMaxFreqHz) {
    m_status = (m_frequencyHz < 80.0f) ? kNoSignal : kInvalid;
    return;
  }

  float rawDist = kMinDistMm + (m_frequencyHz - kMinFreqHz) * kSlope;

#if PULSE_FEATURE_SENSOR_FILTER
  // --- EMA with clamp ---
  if (m_emaDistanceMm < 0.1f) {
    m_emaDistanceMm = rawDist;
    m_distanceMm = rawDist;
  } else {
    float prev = m_emaDistanceMm;
    m_emaDistanceMm = prev * (1.0f - kEmaAlpha) + rawDist * kEmaAlpha;
    float delta = m_emaDistanceMm - m_distanceMm;
    if (delta > 50.0f)        m_emaDistanceMm = m_distanceMm + 50.0f;
    else if (delta < -50.0f)  m_emaDistanceMm = m_distanceMm - 50.0f;
    m_distanceMm = m_emaDistanceMm;
  }
#else
  m_distanceMm = rawDist;
#endif

  if (m_distanceMm < kMinDistMm) m_distanceMm = kMinDistMm;
  if (m_distanceMm > kMaxDistMm) m_distanceMm = kMaxDistMm;
  m_status = kOk;
}

float Q5X_PFM::getFrequencyHz() const    { return m_frequencyHz; }
float Q5X_PFM::getDistanceMm() const     { return m_distanceMm; }
uint32_t Q5X_PFM::getPeriodUs() const    { return m_readPeriodUs; }
uint32_t Q5X_PFM::getRejected() const    { return m_isrRejected; }
Q5X_PFM::Status Q5X_PFM::getStatus() const { return m_status; }
