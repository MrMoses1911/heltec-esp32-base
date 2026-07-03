#ifndef PULSE_SAFEDOCK_SHARED_APP_EVENTS_H
#define PULSE_SAFEDOCK_SHARED_APP_EVENTS_H

#include <cstdint>

namespace pulse {
namespace shared {

enum class SystemEventId : uint16_t {
  kNone = 0,
  kConfigLoaded = 1,
  kConfigSaved = 2,
  kStorageFault = 3,
  kEnterApMode = 4,
  kExitApMode = 5,
  kRestartRequested = 6,
  kStateChanged = 7,
  kStorageReady = 8,
  kModbusReady = 9,
  kLoRaReady = 10,
  kIoReady = 11,
  kIoUpdated = 12,
  kNetworkReady = 13,
  kCommandRouted = 14,
  kLoRaModbusMasterDataReceived = 15,
  kLoRaModbusMasterPollRequestReceived = 16,
  kLoRaModbusMasterWriteCmdReceived = 17,
  kModbusMasterDataReady = 18,
};

struct SystemEvent {
  SystemEventId id = SystemEventId::kNone;
  uint16_t argument = 0;
};

}  // namespace shared
}  // namespace pulse

#endif
