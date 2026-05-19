#include "comms/lora_manager.h"

namespace pulse {
namespace comms {

LoRaManager* LoRaManager::activeInstance_ = nullptr;
volatile bool LoRaManager::rxPending_ = false;

LoRaManager::LoRaManager()
  : systemEventQueue_(nullptr),
    txQueue_(nullptr),
    configMutex_(nullptr),
    taskHandle_(nullptr),
    configSnapshot_{},
    sequenceCounter_(0),
    lastErrorCode_(0),
    lastRssi_(0),
    lastSnrX100_(0),
    rxPacketCounter_(0),
    txPacketCounter_(0),
    lastRxMillis_(0),
    lastHeartbeatMillis_(0),
    commandPending_(false),
    telemetryPending_(false),
    bindingRequestPending_(false),
    bindingResponsePending_(false),
    ackStatusPending_(false),
    groupKeyTransferPending_(false),
    modbusMasterDataPending_(false),
    modbusMasterPollRequestPending_(false),
    modbusMasterWriteCmdPending_(false),
    radioInitialized_(false),
    radioReady_(false),
    initialized_(false) {
  activeInstance_ = this;
}

LoRaManager::~LoRaManager() {
  if (activeInstance_ == this) activeInstance_ = nullptr;
}

esp_err_t LoRaManager::Initialize(QueueHandle_t systemEventQueue, const shared::DeviceConfig& config) {
  systemEventQueue_ = systemEventQueue;
  configSnapshot_ = BuildRuntimeConfig(config);
  initialized_ = true;
  return ESP_OK;
}

bool LoRaManager::QueueBasicTelemetry(const BasicTelemetryPayload& payload, uint16_t destinationAlias) {
  return false;
}

bool LoRaManager::QueueOutputCommand(uint16_t destinationAlias, const OutputCommandPayload& payload) {
  return false;
}

bool LoRaManager::QueueBindingRequest(const BindingRequestPayload& payload) {
  return false;
}

bool LoRaManager::QueueBindingResponse(uint16_t destinationAlias, const BindingResponsePayload& payload, uint8_t* reservedSequence) {
  return false;
}

bool LoRaManager::QueueAckStatus(uint16_t destinationAlias, LoRaMessageType acknowledgedType, uint8_t acknowledgedSequence, shared::AppCommandResult result, uint8_t detailCode) {
  return false;
}

bool LoRaManager::QueueGroupKeyTransfer(
    uint16_t destinationAlias,
    const GroupKeyTransferPayload& payload,
    const std::array<uint16_t, shared::kDeviceUidWordCount>& requesterUidFullWords) {
  return false;
}

bool LoRaManager::QueueModbusMasterData(uint16_t destinationAlias, const ModbusMasterDataPayload& payload) {
  return false;
}

bool LoRaManager::QueueModbusMasterPollRequest(uint16_t destinationAlias) {
  return false;
}

bool LoRaManager::QueueModbusMasterWriteCmd(uint16_t destinationAlias, const ModbusMasterWriteCmdPayload& payload) {
  return false;
}

bool LoRaManager::UpdateConfig(const shared::DeviceConfig& config) {
  return false;
}

bool LoRaManager::ConsumeCommand(shared::AppCommand* command) {
  return false;
}

bool LoRaManager::ConsumeTelemetry(ReceivedTelemetryFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeBindingRequest(ReceivedBindingRequestFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeBindingResponse(ReceivedBindingResponseFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeAckStatus(ReceivedAckStatusFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeGroupKeyTransfer(ReceivedGroupKeyTransferFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeModbusMasterData(ReceivedModbusMasterDataFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeModbusMasterPollRequest(ReceivedModbusMasterPollRequestFrame* frame) {
  return false;
}

bool LoRaManager::ConsumeModbusMasterWriteCmd(ReceivedModbusMasterWriteCmdFrame* frame) {
  return false;
}

void LoRaManager::ResetPairingState() {
}

uint16_t LoRaManager::LastErrorCode() const {
  return lastErrorCode_;
}

bool LoRaManager::GetRuntimeStatus(shared::LoRaRuntimeStatus* status) const {
  if (status == nullptr) return false;
  status->lastRssi = lastRssi_;
  status->lastSnrX100 = lastSnrX100_;
  status->rxPacketCounter = rxPacketCounter_;
  status->txPacketCounter = txPacketCounter_;
  status->lastRxMillis = lastRxMillis_;
  return true;
}

uint32_t LoRaManager::LastHeartbeatMillis() const {
  return lastHeartbeatMillis_;
}

void LoRaManager::TaskEntry(void* context) {
  auto* self = static_cast<LoRaManager*>(context);
  self->Run();
}

void LoRaManager::OnReceiveInterrupt() {
  rxPending_ = true;
}

LoRaManager::LoRaRuntimeConfig LoRaManager::BuildRuntimeConfig(const shared::DeviceConfig& config) {
  LoRaRuntimeConfig rc;
  rc.deviceAlias = config.deviceAlias;
  rc.deviceUidFullWords = config.deviceUidFullWords;
  rc.gatewayCommunicationMode = config.gatewayCommunicationMode;
  rc.loraFrequencyMhzX10 = config.loraFrequencyMhzX10;
  rc.loraBandwidthCode = config.loraBandwidthCode;
  rc.loraSpreadingFactor = config.loraSpreadingFactor;
  rc.loraCodingRateCode = config.loraCodingRateCode;
  rc.loraTxPowerDbm = config.loraTxPowerDbm;
  rc.whitelistSlots = config.whitelistSlots;
  rc.groupKeyValid = config.groupKeyValid;
  rc.groupKeyWords = config.groupKeyWords;
  return rc;
}

bool LoRaManager::ConfigureRadio(const LoRaRuntimeConfig& config) {
  return false;
}

bool LoRaManager::CopyConfig(LoRaRuntimeConfig* config) const {
  if (config == nullptr) return false;
  *config = configSnapshot_;
  return true;
}

void LoRaManager::PublishEvent(shared::SystemEventId id, uint16_t argument) const {
  if (systemEventQueue_ != nullptr) {
    shared::SystemEvent event{id, argument};
    xQueueSend(systemEventQueue_, &event, 0);
  }
}

void LoRaManager::ProcessPendingReceive() {
}

bool LoRaManager::ReserveSequence(uint8_t* sequence) {
  return false;
}

void LoRaManager::Run() {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void LoRaManager::SetLastErrorCode(int errorCode) {
  lastErrorCode_ = static_cast<uint16_t>(errorCode);
}

}  // namespace comms
}  // namespace pulse
