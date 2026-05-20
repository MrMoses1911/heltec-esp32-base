#ifndef PULSE_SAFEDOCK_SYSTEM_NETWORK_MANAGER_H
#define PULSE_SAFEDOCK_SYSTEM_NETWORK_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "shared/app_commands.h"
#include "shared/app_events.h"
#include "shared/app_types.h"
#include "shared/io_snapshot.h"
#include "storage/config_repository.h"

namespace pulse {
namespace system {

class NetworkManager {
 public:
  NetworkManager();
  ~NetworkManager();

  esp_err_t Initialize(QueueHandle_t systemEventQueue, const shared::DeviceConfig& config);
  esp_err_t SetApModeEnabled(bool enabled);
  bool QueueCommand(const shared::AppCommand& command);
  bool ConsumeCommand(shared::AppCommand* command);
  bool ConsumeApModeChange(bool* apModeActive);
  bool ConsumeConfigUpdate(shared::DeviceConfig* config);
  void NotifyCommandResult(shared::AppCommandType type, shared::AppCommandResult result);
  void UpdateSnapshot(
      shared::SystemState state,
      const shared::DeviceConfig& config,
      const shared::IoSnapshot& ioSnapshot,
      const shared::LoRaRuntimeStatus& loraRuntimeStatus);
  uint32_t LastHeartbeatMillis() const;
  void UpdateRemoteSnapshots(
      const std::array<shared::RemoteTelemetrySnapshot, shared::kMaxRxBufferEntries>& remoteTelemetry);
  void UpdateDiagnostics(const shared::RuntimeDiagnostics& diagnostics);
  void UpdateAutoPairingStatus(const shared::AutoPairingStatus& status);
  void UpdateGroupPairingStatus(const shared::GroupPairingStatus& status);
  void GetCurrentApSsid(char* buffer, size_t bufferSize) const;
  uint32_t GetApAutoStopRemainingSeconds() const;
  void UpdateSensorData(float distanceMm, float freqHz, bool valid);
  void UpdateSensorLimits(uint16_t freqMin, uint16_t freqMax, uint16_t distMin, uint16_t distMax);
  void SetConfigRepository(pulse::storage::ConfigRepository* repo);
  void SetApSsidCode(uint16_t code);
  uint16_t GetApSsidCode() const;

 private:
  struct NetworkRequest {
    shared::AppCommand command;
  };

  static esp_err_t IoConfigHandler(httpd_req_t* request);
  static esp_err_t IoOutputsHandler(httpd_req_t* request);
  static esp_err_t IoPulseHandler(httpd_req_t* request);
  static esp_err_t IoClearCountersHandler(httpd_req_t* request);
  static esp_err_t IoMirrorDisarmHandler(httpd_req_t* request);
  static esp_err_t DeviceConfigHandler(httpd_req_t* request);
  static esp_err_t OtaUploadHandler(httpd_req_t* request);
  static esp_err_t RestartHandler(httpd_req_t* request);
  static esp_err_t FactoryResetHandler(httpd_req_t* request);
  static esp_err_t BindingDiscoveryHandler(httpd_req_t* request);
  static esp_err_t GroupPairingHandler(httpd_req_t* request);
  static esp_err_t ModbusMasterTriggerHandler(httpd_req_t* request);
  static esp_err_t LoginHandler(httpd_req_t* request);
  static esp_err_t LogoutHandler(httpd_req_t* request);
  static esp_err_t AuthStatusHandler(httpd_req_t* request);

  static void TaskEntry(void* context);
  static void HandleWifiEvent(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
  static esp_err_t RootHandler(httpd_req_t* request);
  static esp_err_t ParametersPageHandler(httpd_req_t* request);
  static esp_err_t StatusHandler(httpd_req_t* request);
  static esp_err_t ExitApHandler(httpd_req_t* request);

  bool ExtractQueryValue(httpd_req_t* request, const char* key, uint16_t* value) const;
  bool ExtractSignedQueryValue(httpd_req_t* request, const char* key, int16_t* value) const;
  bool ExtractStringQueryValue(httpd_req_t* request, const char* key, char* value, size_t valueSize) const;
  bool ExtractFormValueU32(const char* formData, const char* key, uint32_t* value) const;
  bool ReadFormBody(httpd_req_t* request, char* value, size_t valueSize) const;
  bool ExtractFormStringValue(const char* formData, const char* key, char* value, size_t valueSize) const;
  bool ExtractFormValue(const char* formData, const char* key, uint16_t* value) const;
  bool ExtractSignedFormValue(const char* formData, const char* key, int16_t* value) const;
  bool IsAuthenticated(httpd_req_t* request);
  bool ValidateCredentialValue(const char* value) const;
  void ClearSession();
  void GenerateSessionToken();
  bool QueueHttpCommand(shared::AppCommandType type, uint16_t value);
  esp_err_t EnsureWifiStack();
  esp_err_t StartApMode();
  esp_err_t StopApMode();
  void BuildApSsid(char* buffer, size_t bufferSize) const;
  void BuildApIp(char* buffer, size_t bufferSize) const;
  void BuildRemoteRuntimeText(char* buffer, size_t bufferSize) const;

  void PublishEvent(shared::SystemEventId id, uint16_t argument) const;
  void Run();

  QueueHandle_t systemEventQueue_;
  QueueHandle_t requestQueue_;
  QueueHandle_t commandQueue_;
  QueueHandle_t configQueue_;
  SemaphoreHandle_t stateMutex_;
  TaskHandle_t taskHandle_;
  httpd_handle_t httpServer_;
  esp_netif_t* apNetif_;
  shared::DeviceConfig configSnapshot_;
  shared::IoSnapshot ioSnapshot_;
  shared::LoRaRuntimeStatus loraRuntimeStatus_;
  std::array<shared::RemoteTelemetrySnapshot, shared::kMaxRxBufferEntries> remoteTelemetrySnapshot_;
  shared::RuntimeDiagnostics diagnostics_;
  shared::AutoPairingStatus autoPairingStatus_;
  shared::GroupPairingStatus groupPairingStatus_;
  shared::SystemState systemState_;
  uint16_t connectedStations_;
  shared::AppCommandResult lastCommandResult_;
  shared::AppCommandType lastCommandType_;
  uint16_t apSsidCode_;
  int64_t apStartedAtUs_;
  int64_t sessionLastSeenUs_;
  std::array<char, 17> sessionToken_;
  volatile uint32_t lastHeartbeatMillis_;
  bool wifiStackReady_;
  bool apModeActive_;
  bool stateChangedPending_;
  bool initialized_;
  float sensorDistanceMm_;
  float sensorFreqHz_;
  bool sensorValid_;
  uint16_t sensorFreqMinHz_;
  uint16_t sensorFreqMaxHz_;
  uint16_t sensorDistanceMinMm_;
  uint16_t sensorDistanceMaxMm_;
  pulse::storage::ConfigRepository* configRepo_;

  static NetworkManager* activeInstance_;
};

}  // namespace system
}  // namespace pulse

#endif
