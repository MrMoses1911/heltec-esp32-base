#ifndef PULSE_SAFEDOCK_COMMS_LORA_PROTOCOL_H
#define PULSE_SAFEDOCK_COMMS_LORA_PROTOCOL_H

#include <cstddef>
#include <cstdint>

#include "shared/build_features.h"

namespace pulse {
namespace comms {

enum class LoRaMessageType : uint8_t {
  kBasicTelemetry = 0x01,
  kCounterSync = 0x02,
  kOutputCommand = 0x03,
  kBindingRequest = 0x04,
  kBindingResponse = 0x05,
  kAckStatus = 0x06,
  kGroupKeyTransfer = 0x07,
  kModbusMasterData = 0x08,
  kModbusMasterPollRequest = 0x09,
  kModbusMasterWriteCmd = 0x0A,
};

struct LoRaHeader {
  uint8_t protocolVersion = 1;
  LoRaMessageType messageType = LoRaMessageType::kBasicTelemetry;
  uint8_t flags = 0;
  uint8_t sequence = 0;
  uint16_t sourceAlias = 0;
  uint16_t destinationAlias = 0xFFFF;
};

struct BasicTelemetryPayload {
  uint8_t deviceVariant = 0;
  uint8_t systemState = 0;
  uint8_t communicationMode = 0;
  uint8_t reserved = 0;
  uint16_t telemetryFlags = 0;
  int16_t ambientTemperatureX100 = 0;
  uint16_t relativeHumidityX100 = 0;
  uint16_t digitalInputModeMask = 0;
  uint16_t digitalInputStateMask = 0;
  uint16_t digitalOutputStateMask = 0;
  uint16_t digitalInputCounters[8] = {0};
  uint16_t deviceUptimeMinutes = 0;
  uint16_t sourceUidFullWords[4] = {0};
};

struct OutputCommandPayload {
  uint16_t outputStateMask = 0;
  uint16_t outputPulseMask = 0;
  uint16_t outputCommandFlags = 0;
  BasicTelemetryPayload senderTelemetry;
};

struct BindingRequestPayload {
  uint8_t pairingMode = 0;
  uint8_t deviceVariant = 0;
  uint16_t advertisedCapabilities = 0;
  uint16_t requesterUidFullWords[4] = {0};
};

struct BindingResponsePayload {
  uint8_t pairingResult = 0;
  uint8_t assignedPeerRole = 0;
  uint16_t responderAlias = 0;
  uint16_t responderUidFullWords[4] = {0};
};

struct AckStatusPayload {
  uint8_t acknowledgedMessageType = 0;
  uint8_t acknowledgedSequence = 0;
  uint8_t acknowledgedResultCode = 0;
  uint8_t acknowledgedDetailCode = 0;
};

struct GroupKeyTransferPayload {
  uint16_t groupKeyWords[8] = {0};
};

struct ModbusMasterDataPayload {
  uint16_t snapshotId = 0;
  uint16_t baseSlot = 0;
  uint32_t validSlotMask[2] = {0, 0};
  uint16_t registers[64] = {0};
};

struct ModbusMasterPollRequestPayload {
  uint16_t reserved = 0;
};

struct ModbusMasterWriteCmdPayload {
  uint8_t slotIndex = 0;
  uint8_t reserved  = 0;
  uint16_t value    = 0;
};

constexpr uint8_t kLoRaProtocolVersion = 7;
constexpr uint16_t kLoRaBroadcastAlias = 0xFFFF;
constexpr size_t kLoRaHeaderSize = 8;
constexpr size_t kBasicTelemetryPayloadSize = 42;
constexpr size_t kOutputCommandPayloadSize = kBasicTelemetryPayloadSize + 6;
constexpr size_t kBindingRequestPayloadSize = 12;
constexpr size_t kBindingResponsePayloadSize = 12;
constexpr size_t kAckStatusPayloadSize = 4;
constexpr size_t kGroupKeyTransferPayloadSize = 16;
constexpr size_t kLoRaEncryptedNonceSize = 4;
constexpr size_t kLoRaEncryptedTagSize = 4;
constexpr size_t kLoRaEncryptedOverhead = kLoRaEncryptedNonceSize + kLoRaEncryptedTagSize;
constexpr size_t kModbusMasterDataWindowWords = 64;
constexpr size_t kModbusMasterDataWindowCount = 2;
constexpr size_t kModbusMasterDataPayloadSize = 140;
constexpr size_t kModbusMasterPollRequestPayloadSize = 2;
constexpr size_t kModbusMasterWriteCmdPayloadSize = 4;
constexpr size_t kLoRaMaxPayloadSize = kModbusMasterDataPayloadSize;

constexpr uint8_t kBindingModeDiscover = 1;
constexpr uint8_t kBindingModeGroupJoin = 2;
constexpr uint8_t kBindingModeGroupShare = 3;
constexpr uint8_t kBindingResultRejected = 0;
constexpr uint8_t kBindingResultAccepted = 1;
constexpr uint8_t kBindingRolePeer = 1;
constexpr uint8_t kBindingRoleCoordinator = 2;
constexpr uint16_t kTelemetryStatusFlagOperational = 0x0001;
constexpr uint16_t kTelemetryStatusFlagDiMirrorEnabled = 0x0002;
constexpr uint16_t kTelemetryStatusFlagModbusMirrorEnabled = 0x0004;
constexpr uint16_t kTelemetryStatusFlagTemperatureAlarm = 0x0008;
constexpr uint16_t kTelemetryStatusFlagHumidityAlarm = 0x0010;
constexpr uint16_t kTelemetryStatusFlagSensorFault = 0x0020;
constexpr uint16_t kTelemetryStatusFlagPinConfiguration = 0x0040;
constexpr uint16_t kOutputCommandFlagModbusRemoteOutputs = 0x0001;
constexpr uint16_t kOutputCommandSourceModeShift = 8;
constexpr uint16_t kBindingCapabilityTelemetry = 0x0001;
constexpr uint16_t kBindingCapabilityInputs = 0x0004;
constexpr uint16_t kBindingCapabilityOutputs = 0x0008;

bool SerializeHeader(const LoRaHeader& header, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeHeader(const uint8_t* buffer, size_t bufferSize, LoRaHeader* header);
bool SerializeBasicTelemetry(const BasicTelemetryPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeBasicTelemetry(const uint8_t* buffer, size_t bufferSize, BasicTelemetryPayload* payload);
bool SerializeOutputCommand(const OutputCommandPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeOutputCommand(const uint8_t* buffer, size_t bufferSize, OutputCommandPayload* payload);
bool SerializeBindingRequest(const BindingRequestPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeBindingRequest(const uint8_t* buffer, size_t bufferSize, BindingRequestPayload* payload);
bool SerializeBindingResponse(const BindingResponsePayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeBindingResponse(const uint8_t* buffer, size_t bufferSize, BindingResponsePayload* payload);
bool SerializeAckStatus(const AckStatusPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeAckStatus(const uint8_t* buffer, size_t bufferSize, AckStatusPayload* payload);
bool SerializeGroupKeyTransfer(const GroupKeyTransferPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeGroupKeyTransfer(const uint8_t* buffer, size_t bufferSize, GroupKeyTransferPayload* payload);
bool SerializeModbusMasterData(const ModbusMasterDataPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeModbusMasterData(const uint8_t* buffer, size_t bufferSize, ModbusMasterDataPayload* payload);
bool SerializeModbusMasterPollRequest(const ModbusMasterPollRequestPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeModbusMasterPollRequest(const uint8_t* buffer, size_t bufferSize, ModbusMasterPollRequestPayload* payload);
bool SerializeModbusMasterWriteCmd(const ModbusMasterWriteCmdPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten);
bool DeserializeModbusMasterWriteCmd(const uint8_t* buffer, size_t bufferSize, ModbusMasterWriteCmdPayload* payload);

}  // namespace comms
}  // namespace pulse

#endif
