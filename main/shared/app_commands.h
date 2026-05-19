#ifndef PULSE_SAFEDOCK_SHARED_APP_COMMANDS_H
#define PULSE_SAFEDOCK_SHARED_APP_COMMANDS_H

#include <array>
#include <cstdint>

namespace pulse {
namespace shared {

enum class AppCommandType : uint16_t {
  kNone = 0,
  kSetOutputsMask = 1,
  kPulseOutputsMask = 2,
  kEnterApMode = 3,
  kExitApMode = 4,
  kRestartDevice = 5,
  kClearDiCounters = 6,
  kFactoryResetDevice = 7,
  kArmPairingWindow = 8,
  kSendTelemetryNow = 9,
  kArmGroupPairingWindow = 10,
  kDisarmIoMirrorLatch = 11,
  kClearDoLatch = 12,
  kSendRemoteDoSet = 13,
  kSendRemoteDoPulse = 14,
  kForceSaveConfig = 15,
  kModbusMasterPollNow = 16,
  kModbusMasterWriteReg = 17,
};

enum class AppCommandSource : uint16_t {
  kUnknown = 0,
  kModbus = 1,
  kLoRa = 2,
  kNetwork = 3,
};

enum class AppCommandResult : uint16_t {
  kAccepted = 1,
  kApplied = 2,
  kRejected = 3,
  kFailed = 4,
};

struct AppCommand {
  AppCommandType type = AppCommandType::kNone;
  AppCommandSource source = AppCommandSource::kUnknown;
  uint16_t value = 0;
  uint16_t auxiliary = 0;
  uint16_t context = 0;
  uint16_t detail = 0;
  uint16_t flags = 0;
  std::array<uint16_t, 4> sourceUidFullWords = {0, 0, 0, 0};
};

}  // namespace shared
}  // namespace pulse

#endif
