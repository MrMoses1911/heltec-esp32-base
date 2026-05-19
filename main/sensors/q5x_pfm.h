#ifndef Q5X_PFM_H
#define Q5X_PFM_H

#include <cstdint>

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
  static constexpr uint8_t kBufSize = 16;
  static constexpr float kEmaAlpha = 0.12f;
  static constexpr float kMadReject = 3.0f;

  static void isrHandler(void *arg);

  int m_gpio = 38;
  bool m_initialized = false;

  float m_frequencyHz = 0.0f;
  float m_distanceMm = 0.0f;
  float m_emaDistanceMm = 0.0f;
  Status m_status = kNotInitialized;

  uint32_t m_readPeriodUs = 0;

  volatile uint32_t m_isrBuf[kBufSize] = {};
  volatile uint8_t m_isrIdx = 0;
  volatile uint8_t m_isrCount = 0;
  volatile bool m_isrNewData = false;
  volatile uint32_t m_isrLastValidUs = 0;
  volatile uint32_t m_isrRejected = 0;
};

#endif
