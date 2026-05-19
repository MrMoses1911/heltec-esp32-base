#include "comms/lora_protocol.h"

namespace pulse {
namespace comms {

namespace {

void WriteU16(uint16_t value, uint8_t* buffer) {
  buffer[0] = static_cast<uint8_t>(value & 0xFF);
  buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

uint16_t ReadU16(const uint8_t* buffer) {
  return static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8);
}

}  // namespace

bool SerializeHeader(const LoRaHeader& header, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  if (buffer == nullptr || bytesWritten == nullptr || bufferSize < kLoRaHeaderSize) {
    return false;
  }
  buffer[0] = header.protocolVersion;
  buffer[1] = static_cast<uint8_t>(header.messageType);
  buffer[2] = header.flags;
  buffer[3] = header.sequence;
  WriteU16(header.sourceAlias, &buffer[4]);
  WriteU16(header.destinationAlias, &buffer[6]);
  *bytesWritten = kLoRaHeaderSize;
  return true;
}

bool DeserializeHeader(const uint8_t* buffer, size_t bufferSize, LoRaHeader* header) {
  if (buffer == nullptr || header == nullptr || bufferSize < kLoRaHeaderSize) {
    return false;
  }
  header->protocolVersion = buffer[0];
  header->messageType = static_cast<LoRaMessageType>(buffer[1]);
  header->flags = buffer[2];
  header->sequence = buffer[3];
  header->sourceAlias = ReadU16(&buffer[4]);
  header->destinationAlias = ReadU16(&buffer[6]);
  return true;
}

bool SerializeBasicTelemetry(const BasicTelemetryPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeBasicTelemetry(const uint8_t* buffer, size_t bufferSize, BasicTelemetryPayload* payload) {
  return false;
}

bool SerializeOutputCommand(const OutputCommandPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeOutputCommand(const uint8_t* buffer, size_t bufferSize, OutputCommandPayload* payload) {
  return false;
}

bool SerializeBindingRequest(const BindingRequestPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeBindingRequest(const uint8_t* buffer, size_t bufferSize, BindingRequestPayload* payload) {
  return false;
}

bool SerializeBindingResponse(const BindingResponsePayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeBindingResponse(const uint8_t* buffer, size_t bufferSize, BindingResponsePayload* payload) {
  return false;
}

bool SerializeAckStatus(const AckStatusPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeAckStatus(const uint8_t* buffer, size_t bufferSize, AckStatusPayload* payload) {
  return false;
}

bool SerializeGroupKeyTransfer(const GroupKeyTransferPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeGroupKeyTransfer(const uint8_t* buffer, size_t bufferSize, GroupKeyTransferPayload* payload) {
  return false;
}

bool SerializeModbusMasterData(const ModbusMasterDataPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeModbusMasterData(const uint8_t* buffer, size_t bufferSize, ModbusMasterDataPayload* payload) {
  return false;
}

bool SerializeModbusMasterPollRequest(const ModbusMasterPollRequestPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeModbusMasterPollRequest(const uint8_t* buffer, size_t bufferSize, ModbusMasterPollRequestPayload* payload) {
  return false;
}

bool SerializeModbusMasterWriteCmd(const ModbusMasterWriteCmdPayload& payload, uint8_t* buffer, size_t bufferSize, size_t* bytesWritten) {
  return false;
}

bool DeserializeModbusMasterWriteCmd(const uint8_t* buffer, size_t bufferSize, ModbusMasterWriteCmdPayload* payload) {
  return false;
}

}  // namespace comms
}  // namespace pulse
