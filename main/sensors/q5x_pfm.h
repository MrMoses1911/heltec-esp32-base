#ifndef Q5X_PFM_H
#define Q5X_PFM_H

#include <cstdint>
#include "driver/pulse_cnt.h"
#include "esp_timer.h"

class Q5X_PFM {
public:
  enum Status : uint8_t {
    kOk = 0,
    kNoSignal = 1,
    kInvalid = 2,
    kNotInitialized = 3,
  };

  void init(int gpio = 38);
  void update();
  void setFilterMode(uint16_t mode);
  float getFrequencyHz() const;
  float getDistanceMm() const;
  uint32_t getPeriodUs() const;
  uint32_t getRejected() const;
  Status getStatus() const;

  static constexpr float kMinFreqHz = 100.0f;
  static constexpr float kMaxFreqHz = 600.0f;
  static constexpr float kMinDistMm = 50.0f;
  static constexpr float kMaxDistMm = 5000.0f;

private:
  static constexpr uint32_t kNoSignalGapUs = 300000;
  static constexpr uint32_t kSampleIntervalMs = 80;
  static constexpr float kEmaAlpha = 0.35f;
  static constexpr float kEmaAlphaBuf = 0.20f;
  static constexpr uint8_t kBufSize = 8;

  static void timerCallback(void* arg);

  bool m_initialized = false;

  float m_frequencyHz = 0.0f;
  float m_distanceMm = 0.0f;
  float m_emaDistanceMm = 0.0f;
  Status m_status = kNotInitialized;

  pcnt_unit_handle_t m_pcntUnit = nullptr;
  esp_timer_handle_t m_timer = nullptr;

  int m_lastCount = 0;
  uint32_t m_readPeriodUs = 0;
  uint32_t m_isrRejected = 0;
  uint16_t m_filterMode = 1;
  float m_distBuf[8] = {0};
  uint8_t m_distBufIdx = 0;
  uint8_t m_distBufCount = 0;  // 0=EMA dir, 1=Buffer+Med+EMA
};

#endif
