#include "sensors/q5x_pfm.h"

#include "Arduino.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static constexpr float kMinDistMm = 40.0f;
static constexpr float kMaxDistMm = 2200.0f;
static constexpr float kFreqRange = Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz;
static constexpr float kDistRange = kMaxDistMm - kMinDistMm;
static constexpr float kSlope = kDistRange / kFreqRange;

static constexpr uint32_t kMinPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMaxFreqHz);
static constexpr uint32_t kMaxPeriodUs = static_cast<uint32_t>(1000000.0f / Q5X_PFM::kMinFreqHz);

void Q5X_PFM::init(int gpio) {
  m_gpio = gpio;
  m_initialized = false;
  m_frequencyHz = 0.0f;
  m_distanceMm = 0.0f;
  m_status = kNotInitialized;
  m_readPeriodUs = 0;

  m_isrLastEdgeUs = 0;
  m_isrIdx = 0;
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

  m_lastAcceptedMm = 0.0f;
  m_spikeCount = 0;

  m_isrLastEdgeUs = static_cast<uint32_t>(esp_timer_get_time());
  m_isrLastValidUs = m_isrLastEdgeUs;
  m_initialized = true;
  m_status = kOk;

  Serial.printf("Q5X_PFM GPIO %d\n", gpio);
}

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
    self->m_isrNewData = true;
  } else if (dtUs < kMinPeriodUs) {
    self->m_isrRejected = self->m_isrRejected + 1u;
  } else {
    self->m_isrLastEdgeUs = nowUs;
  }
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

  uint32_t med = medianPeriod();
  if (med == 0) return;

  m_readPeriodUs = med;
  m_frequencyHz = 1000000.0f / static_cast<float>(med);

  if (m_frequencyHz >= Q5X_PFM::kMinFreqHz && m_frequencyHz <= Q5X_PFM::kMaxFreqHz) {
    float rawDist = kMinDistMm + (m_frequencyHz - Q5X_PFM::kMinFreqHz) * kSlope;
    m_status = kOk;

    if (m_lastAcceptedMm < 0.1f) {
      m_distanceMm = rawDist;
      m_lastAcceptedMm = rawDist;
      m_spikeCount = 0;
    } else {
      float delta = rawDist - m_lastAcceptedMm;
      if (delta < 0) delta = -delta;

      if (delta > kMaxDeltaMm) {
        m_spikeCount++;
        if (m_spikeCount >= kSpikeThreshold) {
          m_distanceMm = rawDist;
          m_lastAcceptedMm = rawDist;
          m_spikeCount = 0;
        } else {
          m_distanceMm = m_lastAcceptedMm;
        }
      } else {
        m_distanceMm = rawDist;
        m_lastAcceptedMm = rawDist;
        m_spikeCount = 0;
      }
    }
  } else if (m_frequencyHz < 80.0f) {
    m_status = kNoSignal;
    m_spikeCount = 0;
  } else {
    m_status = kInvalid;
    m_spikeCount = 0;
  }

  Serial.printf("[PFM] period=%lu us freq=%.2f Hz dist=%.1f mm spikes=%u ok=%lu rej=%lu\n",
    static_cast<unsigned long>(med),
    static_cast<double>(m_frequencyHz),
    static_cast<double>(m_distanceMm),
    static_cast<unsigned>(m_spikeCount),
    static_cast<unsigned long>(m_isrValidPulses),
    static_cast<unsigned long>(m_isrRejected));
}

uint32_t Q5X_PFM::medianPeriod() {
  uint32_t buf[kBufSize], n = 0;
  for (uint8_t i = 0; i < kBufSize; i++) {
    uint32_t v = m_isrBuf[i];
    if (v > 0) buf[n++] = v;
  }
  if (n == 0) return 0;
  for (uint8_t i = 0; i < n - 1; i++) {
    for (uint8_t j = 0; j < n - 1 - i; j++) {
      if (buf[j] > buf[j + 1]) { uint32_t t = buf[j]; buf[j] = buf[j + 1]; buf[j + 1] = t; }
    }
  }
  return buf[n / 2];
}

float Q5X_PFM::getFrequencyHz() const { return m_frequencyHz; }
float Q5X_PFM::getDistanceMm() const { return m_distanceMm; }
uint32_t Q5X_PFM::getPeriodUs() const { return m_readPeriodUs; }
uint32_t Q5X_PFM::getRejected() const { return m_isrRejected; }
Q5X_PFM::Status Q5X_PFM::getStatus() const { return m_status; }
