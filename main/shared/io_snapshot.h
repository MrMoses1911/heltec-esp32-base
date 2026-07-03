#ifndef PULSE_SAFEDOCK_SHARED_IO_SNAPSHOT_H
#define PULSE_SAFEDOCK_SHARED_IO_SNAPSHOT_H

#include <cstdint>

namespace pulse {
namespace shared {

struct IoSnapshot {
  int16_t temperatureX100 = 0;
  uint16_t humidityX100 = 0;
  uint16_t diPresentMask = 0;
  uint16_t doPresentMask = 0;
  uint16_t diModeMask = 0;
  uint16_t doModeMask = 0;
  uint16_t diDiscreteStateMask = 0;
  uint16_t doStateMask = 0;
  uint16_t ioAlarmFlags = 0;
  uint16_t localAlarmFlags = 0;
  uint16_t doPulseTriggerMask = 0;
  uint32_t diCounters[8] = {0};
};

}  // namespace shared
}  // namespace pulse

#endif
