#include "sensors/q5x_pfm.h"
#include "Arduino.h"
#include "esp_log.h"
#include "shared/build_features.h"

static constexpr char kLogTag[] = "Q5X_PFM";

static constexpr float kFreqRange = Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz;
static constexpr float kDistRange = Q5X_PFM::kMaxDistMm - Q5X_PFM::kMinDistMm;
static constexpr float kSlope = kDistRange / kFreqRange;

void Q5X_PFM::init(int gpio) {
  m_initialized = false;
  m_frequencyHz = 0.0f;
  m_distanceMm = 0.0f;
  m_emaDistanceMm = 0.0f;
  m_status = kNotInitialized;
  m_readPeriodUs = 0;
  m_lastCount = 0;
  m_isrRejected = 0;

  // ── PCNT unit: count rising edges on GPIO ────────────────
  pcnt_unit_config_t unitConfig = {
    .low_limit = -1,
    .high_limit = 32767,
    .intr_priority = 0,
    .flags = { .accum_count = 0 },
  };
  ESP_ERROR_CHECK(pcnt_new_unit(&unitConfig, &m_pcntUnit));

  pcnt_chan_config_t chanConfig = {
    .edge_gpio_num = gpio,
    .level_gpio_num = -1,
    .flags = {},
  };
  pcnt_channel_handle_t channel = nullptr;
  ESP_ERROR_CHECK(pcnt_new_channel(m_pcntUnit, &chanConfig, &channel));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(channel,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,
    PCNT_CHANNEL_EDGE_ACTION_HOLD));

  pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 1000 };
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(m_pcntUnit, &filter));

  ESP_ERROR_CHECK(pcnt_unit_enable(m_pcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(m_pcntUnit));
  ESP_ERROR_CHECK(pcnt_unit_start(m_pcntUnit));

  // ── Timer TASK (not ISR) to sample count ─────────────────
  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = &Q5X_PFM::timerCallback;
  timerArgs.arg = this;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "pcnt_sample";
  ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &m_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(m_timer, kSampleIntervalMs * 1000ULL));

  m_initialized = true;
  m_status = kOk;
  ESP_LOGI(kLogTag, "PCNT GPIO %d interval=%ums high_limit=%d",
           gpio, kSampleIntervalMs, 32767);
}

void Q5X_PFM::timerCallback(void* arg) {
  auto* self = static_cast<Q5X_PFM*>(arg);
  if (!self || !self->m_initialized) return;

  int count = 0;
  pcnt_unit_get_count(self->m_pcntUnit, &count);

  int delta = count - self->m_lastCount;
  self->m_lastCount = count;

  // Wrap-around: counter range is [-1, 32767], total = 32769 values
  if (delta < 0) delta += 32769;

  float sampleSec = kSampleIntervalMs / 1000.0f;
  float freqHz = static_cast<float>(delta) / sampleSec;

  if (freqHz < 0.1f) {
    self->m_status = kNoSignal;
    return;
  }
  if (freqHz < kMinFreqHz || freqHz > kMaxFreqHz) {
    self->m_status = (freqHz < 80.0f) ? kNoSignal : kInvalid;
    return;
  }

  self->m_frequencyHz = freqHz;
  self->m_readPeriodUs = static_cast<uint32_t>(1000000.0f / freqHz);
  float rawDist = kMinDistMm + (freqHz - kMinFreqHz) * kSlope;

#if PULSE_FEATURE_SENSOR_FILTER
  if (self->m_filterMode == 0) {
    // Mode 0: EMA α=0.35 with clamp
    if (self->m_emaDistanceMm < 0.1f) {
      self->m_emaDistanceMm = rawDist;
      self->m_distanceMm = rawDist;
    } else {
      self->m_emaDistanceMm = self->m_emaDistanceMm * (1.0f - kEmaAlpha) + rawDist * kEmaAlpha;
      float deltaD = self->m_emaDistanceMm - self->m_distanceMm;
      if (deltaD > 200.0f)      self->m_emaDistanceMm = self->m_distanceMm + 200.0f;
      else if (deltaD < -200.0f) self->m_emaDistanceMm = self->m_distanceMm - 200.0f;
      self->m_distanceMm = self->m_emaDistanceMm;
    }
  } else {
    // Mode 1: Buffer + Median + EMA α=0.20
    self->m_distBuf[self->m_distBufIdx] = rawDist;
    self->m_distBufIdx = (self->m_distBufIdx + 1) % kBufSize;
    if (self->m_distBufCount < kBufSize) self->m_distBufCount++;

    if (self->m_distBufCount >= 4) {
      float sorted[kBufSize];
      uint8_t n = self->m_distBufCount;
      for (uint8_t i = 0; i < n; i++) sorted[i] = self->m_distBuf[i];
      // insertion sort for small array
      for (uint8_t i = 1; i < n; i++) {
        float key = sorted[i];
        int8_t j = i - 1;
        while (j >= 0 && sorted[j] > key) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = key;
      }
      // discard top & bottom 25% (2 each for 8)
      uint8_t trim = n / 4;
      uint8_t start = trim;
      uint8_t end = n - trim;
      // median of trimmed range
      uint8_t medIdx = start + (end - start) / 2;
      float med = sorted[medIdx];

      if (self->m_emaDistanceMm < 0.1f) {
        self->m_emaDistanceMm = med;
        self->m_distanceMm = med;
      } else {
        self->m_emaDistanceMm = self->m_emaDistanceMm * (1.0f - kEmaAlphaBuf) + med * kEmaAlphaBuf;
        self->m_distanceMm = self->m_emaDistanceMm;
      }
    }
  }
#else
  self->m_distanceMm = rawDist;
#endif

  if (self->m_distanceMm < kMinDistMm) self->m_distanceMm = kMinDistMm;
  if (self->m_distanceMm > kMaxDistMm) self->m_distanceMm = kMaxDistMm;
  self->m_status = kOk;
}

void Q5X_PFM::setFilterMode(uint16_t mode) {
  m_filterMode = (mode > 0) ? 1 : 0;
  m_emaDistanceMm = 0;  // reset EMA to force re-acquisition
  m_distBufIdx = 0;
  m_distBufCount = 0;
}

void Q5X_PFM::update() {
  // PCNT + timer TASK handles all measurements asynchronously.
  // This function is kept for API compatibility.
}

float Q5X_PFM::getFrequencyHz()    const { return m_frequencyHz; }
float Q5X_PFM::getDistanceMm()     const { return m_distanceMm; }
uint32_t Q5X_PFM::getPeriodUs()    const { return m_readPeriodUs; }
uint32_t Q5X_PFM::getRejected()    const { return m_isrRejected; }
Q5X_PFM::Status Q5X_PFM::getStatus() const { return m_status; }
