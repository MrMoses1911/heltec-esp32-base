#ifndef PULSE_SAFEDOCK_COMMS_LORA_MANAGER_H
#define PULSE_SAFEDOCK_COMMS_LORA_MANAGER_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "comms/lora_protocol.h"
#include "shared/app_commands.h"
#include "shared/app_events.h"
#include "shared/app_types.h"
#include "shared/build_features.h"

namespace pulse {
namespace comms {

struct ReceivedTelemetryFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  bool payloadEncrypted = false;
  BasicTelemetryPayload payload;
};

struct ReceivedBindingRequestFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  BindingRequestPayload payload;
};

struct ReceivedBindingResponseFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  BindingResponsePayload payload;
};

struct ReceivedAckStatusFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  AckStatusPayload payload;
};

struct ReceivedGroupKeyTransferFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  GroupKeyTransferPayload payload;
};

struct ReceivedModbusMasterDataFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  int16_t rssi = 0;
  int16_t snrX100 = 0;
  ModbusMasterDataPayload payload;
};

struct ReceivedModbusMasterPollRequestFrame {
  uint16_t sourceAlias = 0;
  uint8_t sequence = 0;
  ModbusMasterPollRequestPayload payload;
};

struct ReceivedModbusMasterWriteCmdFrame {
  uint16_t sourceAlias = 0;
  uint16_t destinationAlias = 0;
  uint8_t sequence = 0;
  ModbusMasterWriteCmdPayload payload;
};

class LoRaManager {
 public:
  struct LoRaRuntimeConfig {
    uint16_t deviceAlias = 1;
    std::array<uint16_t, shared::kDeviceUidWordCount> deviceUidFullWords = {0, 0, 0, 0};
    shared::GatewayCommunicationMode gatewayCommunicationMode = shared::GatewayCommunicationMode::kDisabled;
    uint16_t loraFrequencyMhzX10 = 9150;
    uint16_t loraBandwidthCode = 500;
    uint16_t loraSpreadingFactor = 9;
    uint16_t loraCodingRateCode = 0;
    int16_t loraTxPowerDbm = 10;
    std::array<shared::WhitelistSlotConfig, shared::kMaxWhitelistSlots> whitelistSlots = {};
    uint16_t groupKeyValid = 0;
    std::array<uint32_t, shared::kGroupKeyWordCount> groupKeyWords = {0, 0, 0, 0};
  };

  LoRaManager();
  ~LoRaManager();

  esp_err_t Initialize(QueueHandle_t systemEventQueue, const shared::DeviceConfig& config);
  bool QueueBasicTelemetry(const BasicTelemetryPayload& payload, uint16_t destinationAlias = kLoRaBroadcastAlias);
  bool QueueOutputCommand(uint16_t destinationAlias, const OutputCommandPayload& payload);
  bool QueueBindingRequest(const BindingRequestPayload& payload);
  bool QueueBindingResponse(uint16_t destinationAlias, const BindingResponsePayload& payload, uint8_t* reservedSequence = nullptr);
  bool QueueAckStatus(uint16_t destinationAlias, LoRaMessageType acknowledgedType, uint8_t acknowledgedSequence, shared::AppCommandResult result, uint8_t detailCode);
  bool QueueGroupKeyTransfer(
      uint16_t destinationAlias,
      const GroupKeyTransferPayload& payload,
      const std::array<uint16_t, shared::kDeviceUidWordCount>& requesterUidFullWords);
  bool QueueModbusMasterData(uint16_t destinationAlias, const ModbusMasterDataPayload& payload);
  bool QueueModbusMasterPollRequest(uint16_t destinationAlias);
  bool QueueModbusMasterWriteCmd(uint16_t destinationAlias, const ModbusMasterWriteCmdPayload& payload);
  bool UpdateConfig(const shared::DeviceConfig& config);
  bool ConsumeCommand(shared::AppCommand* command);
  bool ConsumeTelemetry(ReceivedTelemetryFrame* frame);
  bool ConsumeBindingRequest(ReceivedBindingRequestFrame* frame);
  bool ConsumeBindingResponse(ReceivedBindingResponseFrame* frame);
  bool ConsumeAckStatus(ReceivedAckStatusFrame* frame);
  bool ConsumeGroupKeyTransfer(ReceivedGroupKeyTransferFrame* frame);
  bool ConsumeModbusMasterData(ReceivedModbusMasterDataFrame* frame);
  bool ConsumeModbusMasterPollRequest(ReceivedModbusMasterPollRequestFrame* frame);
  bool ConsumeModbusMasterWriteCmd(ReceivedModbusMasterWriteCmdFrame* frame);
  void ResetPairingState();
  uint16_t LastErrorCode() const;
  bool GetRuntimeStatus(shared::LoRaRuntimeStatus* status) const;
  uint32_t LastHeartbeatMillis() const;

 private:
  enum class TxFrameType : uint8_t {
    kBasicTelemetry = 0,
    kOutputCommand = 1,
    kBindingRequest = 2,
    kBindingResponse = 3,
    kAckStatus = 4,
    kGroupKeyTransfer = 5,
    kModbusMasterData = 6,
    kModbusMasterPollRequest = 7,
    kModbusMasterWriteCmd = 8,
  };

  struct TxFrame {
    TxFrameType type = TxFrameType::kBasicTelemetry;
    uint16_t destinationAlias = kLoRaBroadcastAlias;
    uint8_t sequence = 0;
    size_t payloadLength = 0;
    std::array<uint8_t, kLoRaMaxPayloadSize> payloadBytes = {};
    std::array<uint16_t, shared::kDeviceUidWordCount> requesterUidFullWords = {0, 0, 0, 0};
  };

  static constexpr size_t kMaxPacketSize =
      kLoRaHeaderSize + kLoRaMaxPayloadSize + kLoRaEncryptedOverhead;

  static void TaskEntry(void* context);
  static void OnReceiveInterrupt();

  static LoRaRuntimeConfig BuildRuntimeConfig(const shared::DeviceConfig& config);
  bool ConfigureRadio(const LoRaRuntimeConfig& config);
  bool CopyConfig(LoRaRuntimeConfig* config) const;
  void PublishEvent(shared::SystemEventId id, uint16_t argument) const;
  void ProcessPendingReceive();
  bool ReserveSequence(uint8_t* sequence);
  void Run();

  QueueHandle_t systemEventQueue_;
  QueueHandle_t txQueue_;
  SemaphoreHandle_t configMutex_;
  TaskHandle_t taskHandle_;
  LoRaRuntimeConfig configSnapshot_;
  shared::AppCommand pendingCommand_;
  ReceivedTelemetryFrame pendingTelemetry_;
  ReceivedBindingRequestFrame pendingBindingRequest_;
  ReceivedBindingResponseFrame pendingBindingResponse_;
  ReceivedAckStatusFrame pendingAckStatus_;
  ReceivedGroupKeyTransferFrame pendingGroupKeyTransfer_;
  ReceivedModbusMasterDataFrame pendingModbusMasterData_;
  ReceivedModbusMasterPollRequestFrame pendingModbusMasterPollRequest_;
  ReceivedModbusMasterWriteCmdFrame pendingModbusMasterWriteCmd_;
  uint8_t sequenceCounter_;
  uint16_t lastErrorCode_;
  int16_t lastRssi_;
  int16_t lastSnrX100_;
  uint16_t rxPacketCounter_;
  uint16_t txPacketCounter_;
  uint32_t lastRxMillis_;
  volatile uint32_t lastHeartbeatMillis_;
  bool commandPending_;
  bool telemetryPending_;
  bool bindingRequestPending_;
  bool bindingResponsePending_;
  bool ackStatusPending_;
  bool groupKeyTransferPending_;
  bool modbusMasterDataPending_;
  bool modbusMasterPollRequestPending_;
  bool modbusMasterWriteCmdPending_;
  bool radioInitialized_;
  bool radioReady_;
  bool initialized_;

  static LoRaManager* activeInstance_;
  static volatile bool rxPending_;

  void SetLastErrorCode(int errorCode);
};

}  // namespace comms
}  // namespace pulse

#endif
