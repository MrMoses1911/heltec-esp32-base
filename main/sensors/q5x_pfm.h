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

  static constexpr float kMinFreqHz = 90.0f;
  static constexpr float kMaxFreqHz = 660.0f;

private:
  static constexpr uint32_t kNoSignalGapUs = 300000;
  static constexpr uint8_t kBufSize = 7;
  static constexpr float kMaxDeltaMm = 5.0f;
  static constexpr uint8_t kSpikeThreshold = 10;

  static void isrHandler(void *arg);
  uint32_t medianPeriod();

  int m_gpio;
  bool m_initialized;

  float m_frequencyHz;
  float m_distanceMm;
  float m_lastAcceptedMm;
  uint8_t m_spikeCount;
  Status m_status;

  volatile uint32_t m_isrLastEdgeUs;
  volatile uint32_t m_isrBuf[kBufSize];
  volatile uint8_t m_isrIdx;
  volatile bool m_isrNewData;
  volatile uint32_t m_isrLastValidUs;
  volatile uint32_t m_isrRejected;
  volatile uint32_t m_isrValidPulses;

  uint32_t m_readPeriodUs;
};

#endif
