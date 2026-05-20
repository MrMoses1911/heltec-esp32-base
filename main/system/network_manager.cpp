#include "system/network_manager.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <array>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#include "system/web_ui_texts.h"

namespace pulse {
namespace system {

namespace {

constexpr const char* kLogTag = "NetworkManager";
constexpr uint32_t kNetworkTaskStackSize = 6144;
constexpr uint32_t kHttpServerStackSize = 8192;
constexpr size_t kStatusResponseChunkSize = 3072;
constexpr size_t kDeviceConfigFormBufferSize = 8192;

struct StatusResponseScratch {
  char ssid[33];
  char apIp[16];
  char configuredSsid[33];
  char configuredApIp[16];
  char remoteRuntimeText[256];
  char responseChunk[kStatusResponseChunkSize];
};

constexpr uint8_t kApChannel = 1;
constexpr uint8_t kApMaxConnections = 4;
constexpr int64_t kSessionTimeoutUs = 10LL * 60LL * 1000000LL;
constexpr const char* kDefaultApPassword = "Pulse123";
constexpr const char* kDefaultApIp = "192.168.10.1";

bool IsNonEmptyFixedString(const char* value, size_t maxLength) {
  if (value == nullptr) return false;
  for (size_t index = 0; index < maxLength; ++index) {
    if (value[index] == '\0') return index > 0;
  }
  return true;
}

int64_t ComputeApAutoStopUs(const shared::DeviceConfig& config) {
  return static_cast<int64_t>(config.webServerTimeoutMinutes) * 60LL * 1000000LL;
}

bool IsValidApSsidValue(const char* value) {
  if (value == nullptr) return false;
  size_t length = 0;
  while (value[length] != '\0') {
    const unsigned char current = static_cast<unsigned char>(value[length]);
    if (current < 32 || current > 126) return false;
    ++length;
  }
  return length <= 32;
}

bool IsValidApPasswordValue(const char* value) {
  if (value == nullptr) return false;
  if (value[0] == '\0') return true;
  size_t length = 0;
  while (value[length] != '\0') {
    const unsigned char current = static_cast<unsigned char>(value[length]);
    if (current < 32 || current > 126) return false;
    ++length;
  }
  return length >= 8 && length <= 63;
}

bool ParseIpv4Address(const char* text, std::array<uint8_t, 4>* octets) {
  if (text == nullptr || octets == nullptr) return false;
  unsigned int a = 0, b = 0, c = 0, d = 0;
  char tail = '\0';
  if (std::sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return false;
  if (a > 255 || b > 255 || c > 255 || d > 255) return false;
  *octets = {static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint8_t>(c), static_cast<uint8_t>(d)};
  return true;
}

bool BuildConfiguredApIpInfo(const shared::DeviceConfig& config, esp_netif_ip_info_t* ipInfo) {
  if (ipInfo == nullptr) return false;
  const char* configuredIp = config.fixedApIp[0] != '\0' ? config.fixedApIp.data() : kDefaultApIp;
  std::array<uint8_t, 4> octets = {192, 168, 10, 1};
  if (!ParseIpv4Address(configuredIp, &octets)) return false;
  IP4_ADDR(&ipInfo->ip, octets[0], octets[1], octets[2], octets[3]);
  IP4_ADDR(&ipInfo->gw, octets[0], octets[1], octets[2], octets[3]);
  IP4_ADDR(&ipInfo->netmask, 255, 255, 255, 0);
  return true;
}

uint16_t ClampU16(uint16_t value, uint16_t minValue, uint16_t maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

bool SendChunk(httpd_req_t* request, char* buffer, size_t bufferSize, const char* format, ...) {
  if (request == nullptr || buffer == nullptr || bufferSize == 0 || format == nullptr) return false;
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, bufferSize, format, args);
  va_end(args);
  if (written < 0 || static_cast<size_t>(written) >= bufferSize) return false;
  return httpd_resp_send_chunk(request, buffer, HTTPD_RESP_USE_STRLEN) == ESP_OK;
}

bool SendChunkRaw(httpd_req_t* request, const char* text) {
  if (request == nullptr || text == nullptr) return false;
  return httpd_resp_send_chunk(request, text, HTTPD_RESP_USE_STRLEN) == ESP_OK;
}

esp_err_t SendBadRequest(httpd_req_t* request) {
  httpd_resp_set_status(request, "400 Bad Request");
  return httpd_resp_send(request, "{\"error\":\"bad_request\"}", HTTPD_RESP_USE_STRLEN);
}

}  // namespace

NetworkManager* NetworkManager::activeInstance_ = nullptr;

NetworkManager::NetworkManager()
    : systemEventQueue_(nullptr),
      requestQueue_(nullptr),
      commandQueue_(nullptr),
      configQueue_(nullptr),
      stateMutex_(nullptr),
      taskHandle_(nullptr),
      httpServer_(nullptr),
      apNetif_(nullptr),
      configSnapshot_{},
      ioSnapshot_{},
      loraRuntimeStatus_{},
      remoteTelemetrySnapshot_{},
      diagnostics_{},
      autoPairingStatus_{},
      groupPairingStatus_{},
      systemState_(shared::SystemState::kBoot),
      connectedStations_(0),
      lastCommandResult_(shared::AppCommandResult::kAccepted),
      lastCommandType_(shared::AppCommandType::kNone),
      apSsidCode_(0),
      apStartedAtUs_(0),
      sessionLastSeenUs_(0),
      sessionToken_{},
      lastHeartbeatMillis_(0),
      wifiStackReady_(false),
      apModeActive_(false),
      stateChangedPending_(false),
      initialized_(false),
      sensorDistanceMm_(0),
      sensorFreqHz_(0),
      sensorValid_(false),
      sensorFreqMinHz_(100),
      sensorFreqMaxHz_(600),
      sensorDistanceMinMm_(50),
      sensorDistanceMaxMm_(5000),
      configRepo_(nullptr) {
  activeInstance_ = this;
}

NetworkManager::~NetworkManager() {
  if (activeInstance_ == this) activeInstance_ = nullptr;
}

esp_err_t NetworkManager::Initialize(QueueHandle_t systemEventQueue, const shared::DeviceConfig& config) {
  if (initialized_) return ESP_OK;
  activeInstance_ = this;
  systemEventQueue_ = systemEventQueue;
  configSnapshot_ = config;
  requestQueue_ = xQueueCreate(4, sizeof(NetworkRequest));
  if (requestQueue_ == nullptr) return ESP_ERR_NO_MEM;
  commandQueue_ = xQueueCreate(4, sizeof(shared::AppCommand));
  if (commandQueue_ == nullptr) return ESP_ERR_NO_MEM;
  configQueue_ = xQueueCreate(4, sizeof(shared::DeviceConfig));
  if (configQueue_ == nullptr) return ESP_ERR_NO_MEM;
  stateMutex_ = xSemaphoreCreateMutex();
  if (stateMutex_ == nullptr) return ESP_ERR_NO_MEM;
  // Task is NOT created here — it starts on-demand when StartApMode is called.
  initialized_ = true;
  return ESP_OK;
}

bool NetworkManager::QueueCommand(const shared::AppCommand& command) {
  if (requestQueue_ == nullptr) return false;
  NetworkRequest request;
  request.command = command;
  return xQueueSend(requestQueue_, &request, pdMS_TO_TICKS(100)) == pdTRUE;
}

esp_err_t NetworkManager::SetApModeEnabled(bool enabled) {
  if (stateMutex_ == nullptr) return ESP_ERR_INVALID_STATE;
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
  const bool currentState = apModeActive_;
  xSemaphoreGive(stateMutex_);
  if (currentState == enabled) return ESP_OK;
  esp_err_t commandErr = ESP_OK;
  if (enabled) {
    commandErr = StartApMode();
  } else {
    commandErr = StopApMode();
  }
  if (commandErr != ESP_OK) {
    ESP_LOGE(kLogTag, "Falha ao %s AP: %s", enabled ? "iniciar" : "parar", esp_err_to_name(commandErr));
    return commandErr;
  }
  return ESP_OK;
}

bool NetworkManager::ConsumeCommand(shared::AppCommand* command) {
  if (command == nullptr || commandQueue_ == nullptr) return false;
  return xQueueReceive(commandQueue_, command, 0) == pdTRUE;
}

bool NetworkManager::ConsumeApModeChange(bool* apModeActive) {
  if (apModeActive == nullptr || stateMutex_ == nullptr) return false;
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return false;
  const bool changed = stateChangedPending_;
  if (changed) {
    *apModeActive = apModeActive_;
    stateChangedPending_ = false;
  }
  xSemaphoreGive(stateMutex_);
  return changed;
}

bool NetworkManager::ConsumeConfigUpdate(shared::DeviceConfig* config) {
  if (config == nullptr || configQueue_ == nullptr) return false;
  return xQueueReceive(configQueue_, config, 0) == pdTRUE;
}

void NetworkManager::NotifyCommandResult(shared::AppCommandType type, shared::AppCommandResult result) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  lastCommandType_ = type;
  lastCommandResult_ = result;
  xSemaphoreGive(stateMutex_);
}

void NetworkManager::UpdateAutoPairingStatus(const shared::AutoPairingStatus& status) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  autoPairingStatus_ = status;
  xSemaphoreGive(stateMutex_);
}

void NetworkManager::UpdateGroupPairingStatus(const shared::GroupPairingStatus& status) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  groupPairingStatus_ = status;
  xSemaphoreGive(stateMutex_);
}

void NetworkManager::UpdateSnapshot(
    shared::SystemState state,
    const shared::DeviceConfig& config,
    const shared::IoSnapshot& ioSnapshot,
    const shared::LoRaRuntimeStatus& loraRuntimeStatus) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  systemState_ = state;
  configSnapshot_ = config;
  ioSnapshot_ = ioSnapshot;
  loraRuntimeStatus_ = loraRuntimeStatus;
  xSemaphoreGive(stateMutex_);
}

uint32_t NetworkManager::LastHeartbeatMillis() const {
  return lastHeartbeatMillis_;
}

void NetworkManager::UpdateRemoteSnapshots(
    const std::array<shared::RemoteTelemetrySnapshot, shared::kMaxRxBufferEntries>& remoteTelemetry) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  remoteTelemetrySnapshot_ = remoteTelemetry;
  xSemaphoreGive(stateMutex_);
}

void NetworkManager::UpdateDiagnostics(const shared::RuntimeDiagnostics& diagnostics) {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return;
  diagnostics_ = diagnostics;
  xSemaphoreGive(stateMutex_);
}

void NetworkManager::GetCurrentApSsid(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) return;
  if (stateMutex_ != nullptr && xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) == pdTRUE) {
    BuildApSsid(buffer, bufferSize);
    xSemaphoreGive(stateMutex_);
    return;
  }
  buffer[0] = '\0';
}

uint32_t NetworkManager::GetApAutoStopRemainingSeconds() const {
  if (stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return 0;
  const bool apActive = apModeActive_;
  const bool alwaysOn = configSnapshot_.webServerAlwaysOn != 0;
  const int64_t startedUs = apStartedAtUs_;
  const int64_t timeoutUs = ComputeApAutoStopUs(configSnapshot_);
  xSemaphoreGive(stateMutex_);
  if (!apActive || alwaysOn || startedUs == 0) return 0;
  const int64_t remainingUs = timeoutUs - (esp_timer_get_time() - startedUs);
  return remainingUs > 0 ? static_cast<uint32_t>(remainingUs / 1000000LL) : 0;
}

void NetworkManager::UpdateSensorData(float distanceMm, float freqHz, bool valid) {
  sensorDistanceMm_ = distanceMm;
  sensorFreqHz_ = freqHz;
  sensorValid_ = valid;
}

void NetworkManager::UpdateSensorLimits(uint16_t freqMin, uint16_t freqMax, uint16_t distMin, uint16_t distMax) {
  sensorFreqMinHz_ = freqMin;
  sensorFreqMaxHz_ = freqMax;
  sensorDistanceMinMm_ = distMin;
  sensorDistanceMaxMm_ = distMax;
}

void NetworkManager::SetConfigRepository(pulse::storage::ConfigRepository* repo) {
  configRepo_ = repo;
}

void NetworkManager::SetApSsidCode(uint16_t code) {
  apSsidCode_ = code;
}

uint16_t NetworkManager::GetApSsidCode() const {
  return apSsidCode_;
}

void NetworkManager::TaskEntry(void* context) {
  NetworkManager* self = static_cast<NetworkManager*>(context);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }
  self->Run();
}

void NetworkManager::HandleWifiEvent(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  (void)arg;
  (void)eventData;
  if (activeInstance_ == nullptr || eventBase != WIFI_EVENT || activeInstance_->stateMutex_ == nullptr) return;
  if (xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(10)) != pdTRUE) return;
  const bool previousApState = activeInstance_->apModeActive_;
  if (eventId == WIFI_EVENT_AP_START) {
    activeInstance_->apModeActive_ = true;
    activeInstance_->apStartedAtUs_ = esp_timer_get_time();
  } else if (eventId == WIFI_EVENT_AP_STOP) {
    activeInstance_->apModeActive_ = false;
    activeInstance_->apStartedAtUs_ = 0;
    activeInstance_->connectedStations_ = 0;
    activeInstance_->ClearSession();
  } else if (eventId == WIFI_EVENT_AP_STACONNECTED) {
    ++activeInstance_->connectedStations_;
  } else if (eventId == WIFI_EVENT_AP_STADISCONNECTED && activeInstance_->connectedStations_ > 0) {
    --activeInstance_->connectedStations_;
  }
  if (previousApState != activeInstance_->apModeActive_) {
    activeInstance_->stateChangedPending_ = true;
  }
  xSemaphoreGive(activeInstance_->stateMutex_);
  if (previousApState != activeInstance_->apModeActive_) {
    activeInstance_->PublishEvent(
        activeInstance_->apModeActive_ ? shared::SystemEventId::kEnterApMode : shared::SystemEventId::kExitApMode, 0);
  }
}

esp_err_t NetworkManager::RootHandler(httpd_req_t* request) {
  static constexpr const char* kPage = R"html(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>Pulse SafeDock | Monitor</title>
</head>
)html"
PULSE_WEB_UI_TEXTLIST_SCRIPT
R"html(
<head>
  <style>
    :root{--bg:#000000;--panel:#0f0f0f;--panel2:#181818;--panel3:#111111;--line:#2a2a2a;--text:#ffffff;--muted:#9a9a9a;--red:#c1121f;--red2:#ef233c;--red3:#7f0f18;}
    *{box-sizing:border-box;}
    body{margin:0;background:var(--bg);color:var(--text);font-family:"Segoe UI",Tahoma,Verdana,sans-serif;}
    .shell{max-width:980px;margin:0 auto;padding:24px;}
    .topbar{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;padding:20px 0 24px;border-bottom:1px solid var(--line);}
    .title h1{margin:0;font-size:2rem;letter-spacing:.04em;text-transform:uppercase;}
    .brand-pulse{color:var(--red);}
    .title h2{margin:10px 0 0;color:#5f5f5f;font-size:1rem;font-weight:600;letter-spacing:.04em;text-transform:uppercase;}
    .title p{margin:10px 0 0;color:var(--muted);max-width:760px;line-height:1.5;}
    .actions{display:flex;gap:10px;flex-wrap:wrap;justify-content:flex-end;}
    button{appearance:none;border:1px solid var(--red);background:var(--red);color:#fff;padding:11px 16px;border-radius:10px;font:inherit;font-weight:700;letter-spacing:.04em;cursor:pointer;text-transform:uppercase;}
    button:hover{background:var(--red2);border-color:var(--red2);}
    button.secondary{background:transparent;border-color:#444;color:#fff;}
    button.secondary:hover{border-color:#666;background:#121212;}
    button:disabled{opacity:.45;cursor:not-allowed;}
    .hidden{display:none !important;}
    .stack{display:grid;gap:18px;}
    .card{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:20px;}
    .card h2{margin:0 0 16px;font-size:1rem;text-transform:uppercase;letter-spacing:.08em;color:#fff;}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;}
    .tile{min-width:0;background:#141414;border:1px solid #262626;border-radius:12px;padding:14px;}
    .tile>span{display:block;color:var(--muted);font-size:.75rem;text-transform:uppercase;letter-spacing:.08em;}
    .tile strong{display:block;margin-top:8px;font-size:1.1rem;line-height:1.35;word-break:break-word;}
    .tile small{display:block;margin-top:8px;color:var(--muted);font-size:.76rem;letter-spacing:.04em;}
    .tile .sensor-value{font-size:1.8rem;font-weight:700;color:#fff;}
    .tile .sensor-unit{font-size:.85rem;color:var(--muted);}
    .tile .sensor-valid{color:#38d27d;}
    .tile .sensor-invalid{color:var(--red);}
    .line{display:flex;justify-content:space-between;gap:12px;padding:12px 0;border-bottom:1px solid #202020;}
    .line:last-child{border-bottom:0;padding-bottom:0;}
    .line .label{color:var(--muted);text-transform:uppercase;letter-spacing:.06em;font-size:.76rem;}
    .line .value{text-align:right;font-weight:700;word-break:break-word;}
    .status-chip{display:inline-flex;align-items:center;gap:6px;padding:4px 8px;border-radius:999px;border:1px solid #343434;background:#111;font-size:.72rem;font-weight:800;letter-spacing:.06em;text-transform:uppercase;color:#ddd;}
    .status-chip::before{content:'';width:8px;height:8px;border-radius:50%;background:#6b6b6b;}
    .status-chip.ok{border-color:#2f6b43;background:rgba(41,97,60,0.2);color:#d9ffe5;}
    .status-chip.ok::before{background:#38d27d;}
    .status-chip.alarm{border-color:rgba(193,18,31,0.7);background:rgba(193,18,31,0.16);color:#fff;}
    .status-chip.alarm::before{background:var(--red2);}
    .mono{font-family:"Consolas","Courier New",monospace;}
    .banner{margin:22px 0;padding:14px 16px;border:1px solid #343434;background:var(--panel2);border-radius:12px;color:#e1e1e1;line-height:1.45;}
    @media (max-width:720px){.shell{padding:16px;}.topbar{flex-direction:column;align-items:stretch;}.actions{justify-content:flex-start;}.actions button{flex:1 1 auto;}.card{padding:16px;}.grid{grid-template-columns:1fr;}}
  </style>
</head>
<body>
  <div class='shell'>
    <header class='topbar'>
      <div class='title'>
        <h1><span class='brand-pulse'>PULSE</span> SafeDock</h1>
        <h2 data-text='view.subtitle'>Monitor Operacional</h2>
      </div>
      <div class='actions'>
        <button id='loginBtn' onclick='promptLogin()' data-text='common.buttons.login'>Login</button>
        <button id='paramsBtn' class='hidden' onclick='window.location.href="/params"' data-text='common.buttons.params'>Configuracoes</button>
        <button id='logoutBtn' class='secondary hidden' onclick='performLogout()' data-text='common.buttons.logout'>Logout</button>
      </div>
    </header>

    <div class='banner' id='authSummary' data-text='view.authLoggedOut'>Modo visualizacao.</div>

    <div class='stack'>
      <section class='card'>
        <h2 data-text='view.wifiTitle'>WiFi e Webserver</h2>
        <div class='grid'>
          <div class='tile'><span data-text='view.wifiState'>Estado do AP</span><strong id='wifiApState'>--</strong></div>
          <div class='tile'><span data-text='view.wifiAlwaysOn'>Webserver sempre ligado</span><strong id='wifiAlwaysOn'>--</strong></div>
          <div class='tile'><span data-text='view.wifiRemaining'>Tempo restante do AP</span><strong id='wifiRemaining'>--</strong></div>
          <div class='tile'><span data-text='view.wifiSsid'>SSID</span><strong id='wifiSsid' class='mono'>--</strong></div>
          <div class='tile'><span data-text='view.wifiIp'>IP local</span><strong id='wifiIp' class='mono'>--</strong></div>
          <div class='tile'><span data-text='view.wifiStations'>Clientes conectados</span><strong id='wifiStations'>--</strong></div>
        </div>
      </section>

      <section class='card'>
        <h2>Sensor</h2>
        <div class='grid'>
          <div class='tile'>
            <span>Distancia</span>
            <strong><span class='sensor-value' id='sensorDistance'>--</span> <span class='sensor-unit'>mm</span></strong>
            <small>Validade <span id='sensorValid' class='status-chip'>--</span></small>
          </div>
          <div class='tile'>
            <span>Frequencia</span>
            <strong><span class='sensor-value' id='sensorFrequency'>--</span> <span class='sensor-unit'>Hz</span></strong>
            <small>Limites <span id='sensorLimits'>--</span></small>
          </div>
        </div>
      </section>

      <section class='card'>
        <h2 data-text='view.runtimeTitle'>Runtime</h2>
        <div class='grid'>
          <div class='tile'><span data-text='view.runState'>Estado do sistema</span><strong id='runState'>--</strong></div>
          <div class='tile'><span data-text='view.runAlias'>Alias</span><strong id='runAlias'>--</strong></div>
          <div class='tile'><span data-text='view.runSoftwareVersion'>Versao de software</span><strong id='runSoftwareVersion'>--</strong></div>
          <div class='tile'><span>Uptime</span><strong id='runUptime'>--</strong></div>
        </div>
      </section>
    </div>
  </div>

  <script>
    let authenticated = false
    let latestData = null

    function t(path, fallback){return window.PulseWebUi ? window.PulseWebUi.get(path, fallback) : fallback}
    function f(path, replacements, fallback){return window.PulseWebUi ? window.PulseWebUi.format(path, replacements, fallback) : fallback}

    function stateName(value){return ({1:'BOOT',2:'SELF_TEST',3:'IDLE',4:'NORMAL',5:'AP_MODE',6:'CONFIG',7:'OTA',8:'DEGRADED',9:'FAULT',10:'RESTART',11:'FACTORY_RESET',12:'BUTTON_MENU',13:'WEBSERVER_START',14:'WEBSERVER_STOP'})[value]||String(value)}
    function boolLabel(value){return Number(value)!==0?'Ligado':'Desligado'}
    function formatSeconds(value){
      const s=Number(value||0)
      if(s<=0)return 'Sem contagem'
      const m=Math.floor(s/60),r=s%60
      return m>0?(String(m)+' min '+String(r)+' s'):(String(r)+' s')
    }
    function formatUptime(value){const m=Number(value||0);return m>0?(String(Math.floor(m/60))+'h '+String(m%60)+'min'):'--'}

    function syncAuthButtons(){
      document.getElementById('loginBtn').classList.toggle('hidden', authenticated)
      document.getElementById('logoutBtn').classList.toggle('hidden', !authenticated)
      document.getElementById('paramsBtn').classList.toggle('hidden', !authenticated)
      document.getElementById('authSummary').textContent = authenticated
        ? t('view.authLoggedIn', 'Sessao autenticada.')
        : t('view.authLoggedOut', 'Modo visualizacao.')
    }

    async function refreshAuth(){
      try{
        const response=await fetch('/api/auth/status')
        if(!response.ok)throw new Error('HTTP '+String(response.status))
        const data=await response.json()
        authenticated=!!data.authenticated
        syncAuthButtons()
        return data
      }catch(error){
        console.error('Falha auth',error)
        authenticated=false
      }
      syncAuthButtons()
      return {authenticated:false}
    }

    async function promptLogin(){
      const user=window.prompt(t('common.prompts.loginUser','Usuario:'),'admin')
      if(user===null)return
      const pass=window.prompt(t('common.prompts.loginPass','Senha:'),'')
      if(pass===null)return
      const body=new URLSearchParams({user:user,pass:pass}).toString()
      const response=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
      if(!response.ok){window.alert(t('common.alerts.loginFailed','Falha no login.'));return}
      await refreshAuth()
    }

    async function performLogout(){
      await fetch('/api/logout',{method:'POST'})
      await refreshAuth()
    }

    function fillOverview(data){
      latestData=data
      document.getElementById('wifiApState').textContent=data.ap_active?'Ativo':'Inativo'
      document.getElementById('wifiAlwaysOn').textContent=boolLabel(data.web_server_always_on)
      document.getElementById('wifiRemaining').textContent=Number(data.web_server_always_on||0)!==0?'Sem timeout':formatSeconds(data.ap_auto_stop_remaining_sec)
      document.getElementById('wifiSsid').textContent=data.ssid||'--'
      document.getElementById('wifiIp').textContent=data.ap_ip||'--'
      document.getElementById('wifiStations').textContent=String(data.stations||0)

      document.getElementById('sensorDistance').textContent=String(data.distance_mm||'--')
      document.getElementById('sensorFrequency').textContent=String(data.frequency_hz||'--')
      const sensorValid=Number(data.sensor_valid||0)!==0
      const validEl=document.getElementById('sensorValid')
      validEl.textContent=sensorValid?'OK':'INVALIDO'
      validEl.className='status-chip '+(sensorValid?'ok':'alarm')
      document.getElementById('sensorLimits').textContent=String(data.freq_min_hz||'--')+' / '+String(data.freq_max_hz||'--')+' Hz  |  '+String(data.distance_min_mm||'--')+' / '+String(data.distance_max_mm||'--')+' mm'

      document.getElementById('runState').textContent=stateName(data.state)
      document.getElementById('runAlias').textContent=String(data.alias||'--')
      document.getElementById('runSoftwareVersion').textContent=String(data.software_version||'--')
      document.getElementById('runUptime').textContent=formatUptime(data.uptime_minutes)
    }

    async function load(){
      await refreshAuth()
      const response=await fetch('/api/status')
      if(!response.ok){console.error('Falha /api/status',response.status);return}
      try{fillOverview(await response.json())}catch(error){console.error('Falha parse',error)}
    }

    if(window.PulseWebUi){window.PulseWebUi.apply()}
    load()
    setInterval(load,250)
  </script>
</body>
</html>)html";

  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::ParametersPageHandler(httpd_req_t* request) {
  static constexpr const char* kPage = R"html(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>Pulse SafeDock | Configuracoes</title>
</head>
)html"
PULSE_WEB_UI_TEXTLIST_SCRIPT
R"html(
<head>
  <style>
    :root{--bg:#000000;--panel:#101010;--panel2:#171717;--line:#292929;--text:#ffffff;--muted:#9a9a9a;--red:#c1121f;--red2:#ef233c;--red3:#7f0f18;}
    *{box-sizing:border-box;}
    body{margin:0;background:var(--bg);color:var(--text);font-family:"Segoe UI",Tahoma,Verdana,sans-serif;}
    .shell{max-width:980px;margin:0 auto;padding:24px;}
    .topbar{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;padding:20px 0 24px;border-bottom:1px solid var(--line);}
    h1{margin:0;font-size:2rem;letter-spacing:.04em;text-transform:uppercase;}
    .brand-pulse{color:var(--red);}
    .subtitle{margin:10px 0 0;color:#5f5f5f;font-size:1rem;font-weight:600;letter-spacing:.04em;text-transform:uppercase;}
    .actions{display:flex;gap:10px;flex-wrap:wrap;justify-content:flex-end;}
    button{appearance:none;border:1px solid var(--red);background:var(--red);color:#fff;padding:11px 16px;border-radius:10px;font:inherit;font-weight:700;letter-spacing:.04em;cursor:pointer;text-transform:uppercase;}
    button:hover{background:var(--red2);border-color:var(--red2);}
    button.secondary{background:transparent;border-color:#444;color:#fff;}
    button.secondary:hover{border-color:#666;background:#121212;}
    button.warn{background:var(--red3);border-color:var(--red3);}
    button:disabled{opacity:.45;cursor:not-allowed;}
    .hidden{display:none !important;}
    .notice{margin:20px 0;padding:14px 16px;border:1px solid #333;border-left:4px solid var(--red);background:var(--panel2);border-radius:12px;color:#dedede;line-height:1.5;}
    .feedback{margin:16px 0 0;padding:14px 16px;border:1px solid #333;border-radius:12px;line-height:1.5;background:#141414;color:#e5e5e5;}
    .feedback.success{border-color:#1d5d34;background:#102016;color:#daf6e3;}
    .feedback.error{border-color:#7f1d1d;background:#261213;color:#ffd8d8;}
    .locked{margin-top:22px;background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:24px;}
    .layout{display:grid;gap:18px;margin-top:22px;}
    .section{background:var(--panel);border:1px solid var(--line);border-radius:16px;overflow:hidden;}
    .section-toggle{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;list-style:none;cursor:pointer;padding:20px;background:linear-gradient(180deg,rgba(255,255,255,.03),rgba(255,255,255,0));}
    .section-toggle::-webkit-details-marker{display:none;}
    .section-toggle::after{content:'+';flex:0 0 auto;font-size:1.4rem;line-height:1;color:var(--red);transform:translateY(2px);}
    .section[open] .section-toggle::after{content:'-';}
    .section-heading{min-width:0;}
    .section-heading h2{margin:0 0 8px;font-size:1rem;text-transform:uppercase;letter-spacing:.08em;}
    .section-heading p{margin:0;color:var(--muted);line-height:1.45;}
    .section-body{padding:0 20px 20px;}
    .grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;}
    label{display:block;color:var(--muted);font-size:.84rem;text-transform:uppercase;letter-spacing:.06em;}
    input,select{width:100%;margin-top:6px;padding:12px;border:1px solid #343434;border-radius:10px;background:#0a0a0a;color:#fff;font:inherit;}
    input:focus,select:focus{outline:2px solid rgba(193,18,31,.2);border-color:var(--red);}
    .inline-actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:16px;}
    .summary{padding:12px 14px;border:1px solid #2a2a2a;border-radius:12px;background:#141414;color:#dfdfdf;line-height:1.45;}
    @media (max-width:980px){.grid{grid-template-columns:1fr;}.topbar{flex-direction:column;align-items:stretch;}}
  </style>
</head>
<body>
  <div class='shell'>
    <header class='topbar'>
      <div>
        <h1><span class='brand-pulse'>PULSE</span> SafeDock</h1>
        <h2 class='subtitle' data-text='params.subtitle'>Configuracoes do gateway</h2>
      </div>
      <div class='actions'>
        <button id='loginBtn' onclick='promptLogin()' data-text='common.buttons.login'>Login</button>
        <button id='viewBtn' onclick='window.location.href="/"' data-text='common.buttons.view'>Visualizacao</button>
        <button id='logoutBtn' class='secondary hidden' onclick='performLogout()' data-text='common.buttons.logout'>Logout</button>
      </div>
    </header>

    <div class='notice' id='authSummary' data-text='params.authLocked'>Acesso restrito.</div>
    <div class='feedback hidden' id='saveFeedback'></div>

    <section class='locked' id='lockedPanel'>
      <h2 data-text='params.lockedTitle'>Pagina protegida</h2>
      <p data-text='params.lockedDescription'>As configuracoes so ficam disponiveis apos autenticacao.</p>
    </section>

    <div class='layout hidden' id='paramsLayout'>
      <details class='section'>
        <summary class='section-toggle'>
          <div class='section-heading'>
            <h2>Limites do Sensor</h2>
            <p>Defina os limites de operacao do sensor de distancia e frequencia.</p>
          </div>
        </summary>
        <div class='section-body'>
          <div class='grid'>
            <label><span>Frequencia minima Hz</span><input id='freqMinHz' type='number' min='1' max='10000' value='100'></label>
            <label><span>Frequencia maxima Hz</span><input id='freqMaxHz' type='number' min='1' max='10000' value='600'></label>
            <label><span>Distancia minima mm</span><input id='distanceMinMm' type='number' min='1' max='10000' value='50'></label>
            <label><span>Distancia maxima mm</span><input id='distanceMaxMm' type='number' min='1' max='10000' value='5000'></label>
          </div>
        </div>
      </details>

      <details class='section'>
        <summary class='section-toggle'>
          <div class='section-heading'>
            <h2 data-text='params.networkTitle'>Rede, Webserver e Seguranca</h2>
            <p data-text='params.networkDescription'>Configuracoes de acesso e credenciais.</p>
          </div>
        </summary>
        <div class='section-body'>
          <div class='grid'>
            <label><span data-text='params.labels.webAlwaysOn'>Web sempre ativo</span><select id='webAlwaysOn'><option value='0'>Nao</option><option value='1'>Sim</option></select></label>
            <label><span data-text='params.labels.webTimeout'>Tempo de webserver ligado min</span><input id='webTimeoutMinutes' type='number' min='1' max='720' value='15'></label>
            <label><span data-text='params.labels.fixedSsid'>SSID fixo do AP</span><input id='fixedApSsid' maxlength='32' placeholder='SafeDock_xxxx'></label>
            <label><span data-text='params.labels.fixedIp'>IP fixo do AP</span><input id='fixedApIp' maxlength='15' placeholder='192.168.10.1'></label>
            <label><span data-text='params.labels.fixedPass'>Senha da rede Wi-Fi</span><input id='fixedApPassword' type='password' maxlength='63' placeholder='Pulse123'></label>
            <label><span data-text='params.labels.webUser'>Novo usuario</span><input id='webUsername' value='admin'></label>
            <label><span data-text='params.labels.webPass'>Nova senha</span><input id='webPassword' type='password' placeholder='nova senha'></label>
            <div class='summary'><span data-text='params.labels.wifiSsidSummary'>SSID configurado:</span> <span id='wifiSsidSummary'>--</span></div>
            <div class='summary'><span data-text='params.labels.wifiIpSummary'>IP configurado:</span> <span id='wifiIpSummary'>--</span></div>
          </div>
        </div>
      </details>

      <details class='section'>
        <summary class='section-toggle'>
          <div class='section-heading'>
            <h2 data-text='params.maintenanceTitle'>Persistencia e Manutencao</h2>
            <p data-text='params.maintenanceDescription'>Salve alteracoes e execute comandos.</p>
          </div>
        </summary>
        <div class='section-body'>
          <div class='inline-actions'>
            <button onclick='saveAll()' data-text='common.buttons.saveAll'>Salvar tudo</button>
            <button class='warn' onclick='restartDevice()' data-text='common.buttons.restart'>Reiniciar</button>
            <button class='warn' onclick='factoryResetDevice()' data-text='common.buttons.factoryReset'>Reset de fabrica</button>
            <button class='secondary' onclick='exitAp()' data-text='common.buttons.exitAp'>Encerrar AP</button>
          </div>
        </div>
      </details>
    </div>
  </div>

  <script>
    let authenticated = false
    let latestData = null

    function t(path, fallback){return window.PulseWebUi ? window.PulseWebUi.get(path, fallback) : fallback}
    function f(path, replacements, fallback){return window.PulseWebUi ? window.PulseWebUi.format(path, replacements, fallback) : fallback}
    function v(id){return document.getElementById(id).value.trim()}
    function qs(data){return new URLSearchParams(data).toString()}

    function showFeedback(message, kind){
      const panel = document.getElementById('saveFeedback')
      panel.textContent = message
      panel.classList.remove('hidden','success','error')
      panel.classList.add(kind === 'error' ? 'error' : 'success')
    }

    function clearFeedback(){
      const panel = document.getElementById('saveFeedback')
      panel.textContent = ''
      panel.classList.add('hidden')
      panel.classList.remove('success','error')
    }

    function syncAuthUi(){
      document.getElementById('loginBtn').classList.toggle('hidden', authenticated)
      document.getElementById('logoutBtn').classList.toggle('hidden', !authenticated)
      document.getElementById('lockedPanel').classList.toggle('hidden', authenticated)
      document.getElementById('paramsLayout').classList.toggle('hidden', !authenticated)
      document.getElementById('authSummary').textContent = authenticated
        ? t('params.authUnlocked', 'Sessao autenticada.')
        : t('params.authLocked', 'Acesso restrito.')
    }

    async function refreshAuth(){
      try{
        const response=await fetch('/api/auth/status')
        if(!response.ok)throw new Error('HTTP '+String(response.status))
        const data=await response.json()
        authenticated=!!data.authenticated
        syncAuthUi()
        if(authenticated&&data.web_username){document.getElementById('webUsername').value=data.web_username}
        return data
      }catch(error){
        console.error('Falha auth',error)
        authenticated=false
      }
      syncAuthUi()
      return {authenticated:false}
    }

    async function promptLogin(){
      const user=window.prompt(t('common.prompts.loginUser','Usuario:'),'admin')
      if(user===null)return
      const pass=window.prompt(t('common.prompts.loginPass','Senha:'),'')
      if(pass===null)return
      const body=new URLSearchParams({user:user,pass:pass}).toString()
      const response=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
      if(!response.ok){window.alert(t('common.alerts.loginFailed','Falha no login.'));return}
      await load()
    }

    async function performLogout(){
      await fetch('/api/logout',{method:'POST'})
      clearFeedback()
      await load()
    }

    async function guardedPost(path,q){
      const response=await fetch(path+(q?'?'+q:''),{method:'POST'})
      if(response.status===401){window.alert(t('common.alerts.authRequired','Login necessario.'));await load();return false}
      if(!response.ok){window.alert(t('common.alerts.requestFailed','Falha na requisicao.')+' HTTP '+String(response.status));return false}
      await load()
      return true
    }

    async function guardedPostForm(path,form){
      const response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:form})
      if(response.status===401){window.alert(t('common.alerts.authRequired','Login necessario.'));await load();return false}
      if(!response.ok){window.alert(t('common.alerts.requestFailed','Falha na requisicao.')+' HTTP '+String(response.status));return false}
      await load()
      return true
    }

    async function saveAll(){
      if(!window.confirm(t('common.alerts.saveConfirm','Confirma salvar?')))return
      clearFeedback()
      const payload={freq_min_hz:v('freqMinHz'),freq_max_hz:v('freqMaxHz'),distance_min_mm:v('distanceMinMm'),distance_max_mm:v('distanceMaxMm'),web_always_on:v('webAlwaysOn'),web_timeout_minutes:v('webTimeoutMinutes'),fixed_ssid:v('fixedApSsid'),fixed_ip:v('fixedApIp'),alias:String(latestData?latestData.alias:1)}
      const webUser=v('webUsername')
      const webPass=v('webPassword')
      if(webUser){payload.web_user=webUser}
      if(webPass){payload.web_pass=webPass}
      const fixedPass=v('fixedApPassword')
      if(fixedPass){payload.fixed_pass=fixedPass}
      const ok=await guardedPostForm('/api/device/config',qs(payload))
      if(ok){
        document.getElementById('fixedApPassword').value=''
        document.getElementById('webPassword').value=''
        showFeedback(t('params.saveSuccess','Configuracoes salvas com sucesso.'),'success')
      } else {
        showFeedback(t('params.saveGeneralError','Nao foi possivel salvar.'),'error')
      }
    }

    async function restartDevice(){
      if(window.confirm(t('common.alerts.restartConfirm','Confirma reiniciar?'))){await guardedPost('/api/device/restart','')}
    }

    async function factoryResetDevice(){
      if(window.confirm(t('common.alerts.factoryResetConfirm','Confirma reset de fabrica?'))){await guardedPost('/api/device/factory-reset','')}
    }

    async function exitAp(){
      if(window.confirm(t('common.alerts.exitApConfirm','Confirma encerrar AP?'))){await guardedPost('/api/exit-ap','')}
    }

    async function load(){
      await refreshAuth()
      if(!authenticated)return
      const response=await fetch('/api/status')
      if(!response.ok){console.error('Falha /api/status',String(response.status));return}
      try{
        latestData=await response.json()
        document.getElementById('wifiSsidSummary').textContent=latestData.configured_ssid||'--'
        document.getElementById('wifiIpSummary').textContent=latestData.configured_ap_ip||'--'
        document.getElementById('freqMinHz').value=String(latestData.freq_min_hz||100)
        document.getElementById('freqMaxHz').value=String(latestData.freq_max_hz||600)
        document.getElementById('distanceMinMm').value=String(latestData.distance_min_mm||50)
        document.getElementById('distanceMaxMm').value=String(latestData.distance_max_mm||5000)
        document.getElementById('webAlwaysOn').value=String(latestData.web_server_always_on||0)
        document.getElementById('webTimeoutMinutes').value=String(latestData.web_server_timeout_minutes||15)
        document.getElementById('fixedApSsid').value=latestData.configured_ssid||''
        document.getElementById('fixedApPassword').value=''
        document.getElementById('fixedApIp').value=latestData.configured_ap_ip||''
      }catch(error){console.error('Falha parse',error)}
    }

    if(window.PulseWebUi){window.PulseWebUi.apply()}
    load()
  </script>
</body>
</html>)html";

  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::StatusHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr || activeInstance_->stateMutex_ == nullptr) {
    return httpd_resp_send_500(request);
  }

  StatusResponseScratch* scratch = static_cast<StatusResponseScratch*>(std::calloc(1, sizeof(StatusResponseScratch)));
  if (scratch == nullptr) return httpd_resp_send_500(request);

  if (xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) {
    std::free(scratch);
    return httpd_resp_send_500(request);
  }

  uint32_t apAutoStopRemainingSec = 0;
  scratch->responseChunk[0] = '\0';
  activeInstance_->BuildApSsid(scratch->ssid, sizeof(scratch->ssid));
  activeInstance_->BuildApIp(scratch->apIp, sizeof(scratch->apIp));
  std::strncpy(scratch->configuredSsid, activeInstance_->configSnapshot_.fixedApSsid.data(), sizeof(scratch->configuredSsid) - 1);
  std::strncpy(scratch->configuredApIp, activeInstance_->configSnapshot_.fixedApIp.data(), sizeof(scratch->configuredApIp) - 1);
  activeInstance_->BuildRemoteRuntimeText(scratch->remoteRuntimeText, sizeof(scratch->remoteRuntimeText));

  const uint32_t freeHeapBytes = esp_get_free_heap_size();
  const uint32_t minFreeHeapBytes = esp_get_minimum_free_heap_size();
  const uint32_t largestFreeBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (activeInstance_->apModeActive_ &&
      activeInstance_->configSnapshot_.webServerAlwaysOn == 0 &&
      activeInstance_->apStartedAtUs_ != 0) {
    const int64_t elapsedUs = esp_timer_get_time() - activeInstance_->apStartedAtUs_;
    const int64_t remainingUs = ComputeApAutoStopUs(activeInstance_->configSnapshot_) - elapsedUs;
    apAutoStopRemainingSec = remainingUs > 0 ? static_cast<uint32_t>(remainingUs / 1000000LL) : 0;
  }

  httpd_resp_set_type(request, "application/json");

  bool responseOk = SendChunk(
      request, scratch->responseChunk, sizeof(scratch->responseChunk),
      "{"
      "\"alias\":%u,"
      "\"device_uid_full\":[%u,%u,%u,%u],"
      "\"device_variant\":%u,"
      "\"software_version\":\"%s\","
      "\"state\":%u,"
      "\"ap_active\":%s,"
      "\"stations\":%u,"
      "\"ssid\":\"%s\","
      "\"ap_ip\":\"%s\","
      "\"configured_ssid\":\"%s\","
      "\"configured_ap_ip\":\"%s\","
      "\"web_server_always_on\":%u,"
      "\"web_server_timeout_minutes\":%u,"
      "\"ap_auto_stop_remaining_sec\":%lu,"
      "\"distance_mm\":%u,"
      "\"frequency_hz\":%u,"
      "\"sensor_valid\":%s,"
      "\"freq_min_hz\":%u,"
      "\"freq_max_hz\":%u,"
      "\"distance_min_mm\":%u,"
      "\"distance_max_mm\":%u,"
      "\"lora_last_rssi\":%d,"
      "\"lora_last_snr_x100\":%d,"
      "\"lora_rx_packets\":%u,"
      "\"lora_tx_packets\":%u,"
      "\"uptime_minutes\":%u,"
      "\"last_command_type\":%u,"
      "\"last_command_result\":%u,"
      "\"last_persist_error\":%u,"
      "\"last_lora_error\":%u,"
      "\"last_general_error\":%u,"
      "\"free_heap_bytes\":%u,"
      "\"min_free_heap_bytes\":%u,"
      "\"largest_free_block_bytes\":%u,"
      "\"remote_runtime_text\":\"%s\","
      "\"auto_pairing_active\":%s,"
      "\"auto_pairing_state\":%u,"
      "\"auto_pairing_remaining_ms\":%u,"
      "\"auto_pairing_peer_valid\":%s,"
      "\"auto_pairing_peer_slot\":%u,"
      "\"auto_pairing_peer_alias\":%u,"
      "\"group_pairing_active\":%s,"
      "\"group_pairing_state\":%u,"
      "\"group_pairing_action\":%u,"
      "\"group_pairing_remaining_ms\":%u,"
      "\"group_pairing_peer_valid\":%s,"
      "\"group_pairing_peer_alias\":%u"
      "}",
      activeInstance_->configSnapshot_.deviceAlias,
      activeInstance_->configSnapshot_.deviceUidFullWords[0],
      activeInstance_->configSnapshot_.deviceUidFullWords[1],
      activeInstance_->configSnapshot_.deviceUidFullWords[2],
      activeInstance_->configSnapshot_.deviceUidFullWords[3],
      static_cast<unsigned>(activeInstance_->configSnapshot_.deviceVariant),
      "1.0.0",
      static_cast<unsigned>(activeInstance_->systemState_),
      activeInstance_->apModeActive_ ? "true" : "false",
      activeInstance_->connectedStations_,
      scratch->ssid,
      scratch->apIp,
      scratch->configuredSsid,
      scratch->configuredApIp,
       activeInstance_->configSnapshot_.webServerAlwaysOn,
       activeInstance_->configSnapshot_.webServerTimeoutMinutes,
       static_cast<unsigned long>(apAutoStopRemainingSec),
       static_cast<unsigned>(activeInstance_->sensorDistanceMm_),
       static_cast<unsigned>(activeInstance_->sensorFreqHz_),
       activeInstance_->sensorValid_ ? "true" : "false",
       activeInstance_->sensorFreqMinHz_,
       activeInstance_->sensorFreqMaxHz_,
       activeInstance_->sensorDistanceMinMm_,
       activeInstance_->sensorDistanceMaxMm_,
        static_cast<int>(activeInstance_->loraRuntimeStatus_.lastRssi),
        static_cast<int>(activeInstance_->loraRuntimeStatus_.lastSnrX100),
       activeInstance_->loraRuntimeStatus_.rxPacketCounter,
      activeInstance_->loraRuntimeStatus_.txPacketCounter,
      0u,
      static_cast<unsigned>(activeInstance_->lastCommandType_),
      static_cast<unsigned>(activeInstance_->lastCommandResult_),
      activeInstance_->diagnostics_.lastPersistError,
      activeInstance_->diagnostics_.lastLoraError,
      activeInstance_->diagnostics_.lastGeneralErrorCode,
      freeHeapBytes,
      minFreeHeapBytes,
      largestFreeBlockBytes,
      scratch->remoteRuntimeText,
      activeInstance_->autoPairingStatus_.active ? "true" : "false",
      static_cast<unsigned>(activeInstance_->autoPairingStatus_.state),
      activeInstance_->autoPairingStatus_.remainingMs,
      activeInstance_->autoPairingStatus_.peer.valid ? "true" : "false",
      activeInstance_->autoPairingStatus_.peer.whitelistSlot,
      activeInstance_->autoPairingStatus_.peer.alias,
      activeInstance_->groupPairingStatus_.active ? "true" : "false",
      static_cast<unsigned>(activeInstance_->groupPairingStatus_.state),
      static_cast<unsigned>(activeInstance_->groupPairingStatus_.action),
      activeInstance_->groupPairingStatus_.remainingMs,
      activeInstance_->groupPairingStatus_.peer.valid ? "true" : "false",
      activeInstance_->groupPairingStatus_.peer.alias);

  xSemaphoreGive(activeInstance_->stateMutex_);

  esp_err_t responseErr = ESP_FAIL;
  if (responseOk) {
    responseErr = httpd_resp_send_chunk(request, nullptr, 0);
  }
  std::free(scratch);
  return responseErr;
}

esp_err_t NetworkManager::ExitApHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr) return httpd_resp_send_500(request);
  if (!activeInstance_->IsAuthenticated(request)) {
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_send(request, "{\"error\":\"auth_required\"}", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_send(request, "{\"stopped\":true}", HTTPD_RESP_USE_STRLEN);
  vTaskDelay(pdMS_TO_TICKS(50));
  activeInstance_->SetApModeEnabled(false);
  return ESP_OK;
}

esp_err_t NetworkManager::AuthStatusHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr) return httpd_resp_send_500(request);
  const bool authenticated = activeInstance_->IsAuthenticated(request);
  uint32_t expiresInSec = 0;
  if (authenticated && activeInstance_->stateMutex_ != nullptr &&
      xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) == pdTRUE) {
    const int64_t remainingUs = kSessionTimeoutUs - (esp_timer_get_time() - activeInstance_->sessionLastSeenUs_);
    expiresInSec = remainingUs > 0 ? static_cast<uint32_t>(remainingUs / 1000000LL) : 0;
    xSemaphoreGive(activeInstance_->stateMutex_);
  }
  char username[16] = {0};
  if (authenticated && activeInstance_->stateMutex_ != nullptr &&
      xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) == pdTRUE) {
    std::strncpy(username, activeInstance_->configSnapshot_.webUsername.data(), sizeof(username) - 1);
    xSemaphoreGive(activeInstance_->stateMutex_);
  }
  char buffer[160] = {0};
  std::snprintf(buffer, sizeof(buffer),
      "{\"authenticated\":%s,\"session_expires_in_sec\":%u,\"web_username\":\"%s\"}",
      authenticated ? "true" : "false", static_cast<unsigned>(expiresInSec),
      authenticated ? username : "");
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request, buffer, HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::LoginHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr || activeInstance_->stateMutex_ == nullptr) return httpd_resp_send_500(request);
  char formData[96] = {0};
  if (!activeInstance_->ReadFormBody(request, formData, sizeof(formData))) return SendBadRequest(request);
  char user[16] = {0};
  char pass[16] = {0};
  if (!activeInstance_->ExtractFormStringValue(formData, "user", user, sizeof(user)) ||
      !activeInstance_->ExtractFormStringValue(formData, "pass", pass, sizeof(pass))) {
    return SendBadRequest(request);
  }
  if (xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) {
    return httpd_resp_send_500(request);
  }
  uint8_t credentialsDiff = 0;
  const char* const storedUser = activeInstance_->configSnapshot_.webUsername.data();
  const char* const storedPass = activeInstance_->configSnapshot_.webPassword.data();
  for (size_t i = 0; i < sizeof(user); ++i) {
    credentialsDiff |= static_cast<uint8_t>(user[i] ^ storedUser[i]);
  }
  for (size_t i = 0; i < sizeof(pass); ++i) {
    credentialsDiff |= static_cast<uint8_t>(pass[i] ^ storedPass[i]);
  }
  const bool valid = credentialsDiff == 0;
  if (valid) {
    activeInstance_->GenerateSessionToken();
    activeInstance_->sessionLastSeenUs_ = esp_timer_get_time();
  }
  xSemaphoreGive(activeInstance_->stateMutex_);
  if (!valid) {
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_send(request, "{\"error\":\"invalid_credentials\"}", HTTPD_RESP_USE_STRLEN);
  }
  char cookie[64] = {0};
  std::snprintf(cookie, sizeof(cookie), "pulse_session=%s; Path=/; HttpOnly; SameSite=Lax",
      activeInstance_->sessionToken_.data());
  httpd_resp_set_hdr(request, "Set-Cookie", cookie);
  return httpd_resp_send(request, "{\"authenticated\":true}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::LogoutHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr) return httpd_resp_send_500(request);
  if (activeInstance_->stateMutex_ != nullptr &&
      xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) == pdTRUE) {
    activeInstance_->ClearSession();
    xSemaphoreGive(activeInstance_->stateMutex_);
  }
  httpd_resp_set_hdr(request, "Set-Cookie", "pulse_session=; Path=/; Max-Age=0; HttpOnly");
  return httpd_resp_send(request, "{\"authenticated\":false}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::DeviceConfigHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr || activeInstance_->stateMutex_ == nullptr) return httpd_resp_send_500(request);
  if (!activeInstance_->IsAuthenticated(request)) {
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_send(request, "{\"error\":\"auth_required\"}", HTTPD_RESP_USE_STRLEN);
  }
  char* formData = static_cast<char*>(std::calloc(1, kDeviceConfigFormBufferSize));
  if (formData == nullptr) return httpd_resp_send_500(request);
  if (!activeInstance_->ReadFormBody(request, formData, kDeviceConfigFormBufferSize)) {
    std::free(formData);
    return SendBadRequest(request);
  }
  if (xSemaphoreTake(activeInstance_->stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) {
    std::free(formData);
    return httpd_resp_send_500(request);
  }
  uint16_t value = 0;
  char webUser[16] = {0};
  char webPass[16] = {0};
  char fixedSsid[33] = {0};
  char fixedIp[16] = {0};
  char fixedPass[64] = {0};
  shared::DeviceConfig updatedConfig = activeInstance_->configSnapshot_;
  bool configChanged = false;
  if (activeInstance_->ExtractFormValue(formData, "alias", &value)) {
    updatedConfig.deviceAlias = ClampU16(value, 1, 65535);
    configChanged = true;
  }
  if (activeInstance_->ExtractFormValue(formData, "web_always_on", &value)) {
    updatedConfig.webServerAlwaysOn = value != 0 ? 1 : 0;
    configChanged = true;
  }
  if (activeInstance_->ExtractFormValue(formData, "web_timeout_minutes", &value)) {
    updatedConfig.webServerTimeoutMinutes = value;
    configChanged = true;
  }
  if (activeInstance_->ExtractFormStringValue(formData, "web_user", webUser, sizeof(webUser))) {
    if (!activeInstance_->ValidateCredentialValue(webUser)) {
      xSemaphoreGive(activeInstance_->stateMutex_);
      std::free(formData);
      return SendBadRequest(request);
    }
    std::strncpy(updatedConfig.webUsername.data(), webUser, updatedConfig.webUsername.size() - 1);
    updatedConfig.webUsername[updatedConfig.webUsername.size() - 1] = '\0';
    configChanged = true;
  }
  if (activeInstance_->ExtractFormStringValue(formData, "web_pass", webPass, sizeof(webPass))) {
    if (!activeInstance_->ValidateCredentialValue(webPass)) {
      xSemaphoreGive(activeInstance_->stateMutex_);
      std::free(formData);
      return SendBadRequest(request);
    }
    std::strncpy(updatedConfig.webPassword.data(), webPass, updatedConfig.webPassword.size() - 1);
    updatedConfig.webPassword[updatedConfig.webPassword.size() - 1] = '\0';
    configChanged = true;
  }
  if (activeInstance_->ExtractFormStringValue(formData, "fixed_ssid", fixedSsid, sizeof(fixedSsid))) {
    if (!IsValidApSsidValue(fixedSsid)) {
      xSemaphoreGive(activeInstance_->stateMutex_);
      std::free(formData);
      return SendBadRequest(request);
    }
    std::strncpy(updatedConfig.fixedApSsid.data(), fixedSsid, updatedConfig.fixedApSsid.size() - 1);
    updatedConfig.fixedApSsid[updatedConfig.fixedApSsid.size() - 1] = '\0';
    configChanged = true;
  }
  if (activeInstance_->ExtractFormStringValue(formData, "fixed_ip", fixedIp, sizeof(fixedIp))) {
    std::array<uint8_t, 4> octets = {0, 0, 0, 0};
    if (!ParseIpv4Address(fixedIp, &octets)) {
      xSemaphoreGive(activeInstance_->stateMutex_);
      std::free(formData);
      return SendBadRequest(request);
    }
    std::strncpy(updatedConfig.fixedApIp.data(), fixedIp, updatedConfig.fixedApIp.size() - 1);
    updatedConfig.fixedApIp[updatedConfig.fixedApIp.size() - 1] = '\0';
    configChanged = true;
  }
  if (activeInstance_->ExtractFormStringValue(formData, "fixed_pass", fixedPass, sizeof(fixedPass))) {
    if (!IsValidApPasswordValue(fixedPass)) {
      xSemaphoreGive(activeInstance_->stateMutex_);
      std::free(formData);
      return SendBadRequest(request);
    }
    updatedConfig.fixedApPassword.fill('\0');
    std::strncpy(updatedConfig.fixedApPassword.data(), fixedPass, updatedConfig.fixedApPassword.size() - 1);
    updatedConfig.fixedApPassword[updatedConfig.fixedApPassword.size() - 1] = '\0';
    configChanged = true;
  }
  // Parse sensor limit fields
  pulse::storage::SafeDockConfig sensorCfg;
  sensorCfg.freqMinHz = 100;
  sensorCfg.freqMaxHz = 600;
  sensorCfg.distanceMinMm = 50;
  sensorCfg.distanceMaxMm = 5000;
  if (activeInstance_->ExtractFormValue(formData, "freq_min_hz", &value)) {
    sensorCfg.freqMinHz = ClampU16(value, 50, 700);
    configChanged = true;
  }
  if (activeInstance_->ExtractFormValue(formData, "freq_max_hz", &value)) {
    sensorCfg.freqMaxHz = ClampU16(value, 50, 700);
    configChanged = true;
  }
  if (activeInstance_->ExtractFormValue(formData, "distance_min_mm", &value)) {
    sensorCfg.distanceMinMm = ClampU16(value, 10, 6000);
    configChanged = true;
  }
  if (activeInstance_->ExtractFormValue(formData, "distance_max_mm", &value)) {
    sensorCfg.distanceMaxMm = ClampU16(value, 10, 6000);
    configChanged = true;
  }
  // Apply changes to configSnapshot_ immediately so /api/status reflects them
  if (configChanged) {
    activeInstance_->configSnapshot_.webServerAlwaysOn = updatedConfig.webServerAlwaysOn;
    activeInstance_->configSnapshot_.webServerTimeoutMinutes = updatedConfig.webServerTimeoutMinutes;
    std::strncpy(activeInstance_->configSnapshot_.webUsername.data(), updatedConfig.webUsername.data(),
                 activeInstance_->configSnapshot_.webUsername.size() - 1);
    std::strncpy(activeInstance_->configSnapshot_.webPassword.data(), updatedConfig.webPassword.data(),
                 activeInstance_->configSnapshot_.webPassword.size() - 1);
    std::strncpy(activeInstance_->configSnapshot_.fixedApSsid.data(), updatedConfig.fixedApSsid.data(),
                 activeInstance_->configSnapshot_.fixedApSsid.size() - 1);
    std::strncpy(activeInstance_->configSnapshot_.fixedApIp.data(), updatedConfig.fixedApIp.data(),
                 activeInstance_->configSnapshot_.fixedApIp.size() - 1);
    std::strncpy(activeInstance_->configSnapshot_.fixedApPassword.data(), updatedConfig.fixedApPassword.data(),
                 activeInstance_->configSnapshot_.fixedApPassword.size() - 1);
  }
  xSemaphoreGive(activeInstance_->stateMutex_);
  std::free(formData);
  if (configChanged) {
    if (activeInstance_->configRepo_ != nullptr) {
      pulse::storage::SafeDockConfig fullCfg;
      activeInstance_->configRepo_->Load(&fullCfg);
      fullCfg.schemaVersion = 1;
      fullCfg.freqMinHz = sensorCfg.freqMinHz;
      fullCfg.freqMaxHz = sensorCfg.freqMaxHz;
      fullCfg.distanceMinMm = sensorCfg.distanceMinMm;
      fullCfg.distanceMaxMm = sensorCfg.distanceMaxMm;
      std::strncpy(fullCfg.webUsername, updatedConfig.webUsername.data(), sizeof(fullCfg.webUsername) - 1);
      std::strncpy(fullCfg.webPassword, updatedConfig.webPassword.data(), sizeof(fullCfg.webPassword) - 1);
      std::strncpy(fullCfg.apSsid, updatedConfig.fixedApSsid.data(), sizeof(fullCfg.apSsid) - 1);
      fullCfg.apSsid[sizeof(fullCfg.apSsid) - 1] = '\0';
      std::strncpy(fullCfg.apPassword, updatedConfig.fixedApPassword.data(), sizeof(fullCfg.apPassword) - 1);
      fullCfg.apPassword[sizeof(fullCfg.apPassword) - 1] = '\0';
      fullCfg.webServerAlwaysOn = updatedConfig.webServerAlwaysOn;
      fullCfg.webServerTimeoutMinutes = updatedConfig.webServerTimeoutMinutes;
      esp_err_t saveErr = activeInstance_->configRepo_->Save(fullCfg);
      if (saveErr != ESP_OK) {
        ESP_LOGE(kLogTag, "Falha ao salvar config: %s", esp_err_to_name(saveErr));
        httpd_resp_set_status(request, "500 Internal Server Error");
        char errBody[64];
        std::snprintf(errBody, sizeof(errBody), "{\"error\":\"save_failed\",\"code\":%d}", saveErr);
        return httpd_resp_send(request, errBody, HTTPD_RESP_USE_STRLEN);
      }
      ESP_LOGI(kLogTag, "Config salva com sucesso");
      // Update NetworkManager sensor limits from what was saved
      activeInstance_->UpdateSensorLimits(
          fullCfg.freqMinHz, fullCfg.freqMaxHz,
          fullCfg.distanceMinMm, fullCfg.distanceMaxMm);
      // Persist SSID code so always-on keeps the same SSID across reboots
      if (activeInstance_->apSsidCode_ != 0) {
        fullCfg.apSsidCode = activeInstance_->apSsidCode_;
        activeInstance_->configRepo_->SaveSsidCode(activeInstance_->apSsidCode_);
      }
    } else {
      ESP_LOGW(kLogTag, "configRepo_ nao definido, config nao salva");
    }
  }
  httpd_resp_set_status(request, "200 OK");
  return httpd_resp_send(request, "{\"saved\":true}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::RestartHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr) return httpd_resp_send_500(request);
  if (!activeInstance_->IsAuthenticated(request)) {
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_send(request, "{\"error\":\"auth_required\"}", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_status(request, "200 OK");
  httpd_resp_send(request, "{\"restarting\":true}", HTTPD_RESP_USE_STRLEN);
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_restart();
  return ESP_OK;
}

esp_err_t NetworkManager::FactoryResetHandler(httpd_req_t* request) {
  if (activeInstance_ == nullptr) return httpd_resp_send_500(request);
  if (!activeInstance_->IsAuthenticated(request)) {
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_send(request, "{\"error\":\"auth_required\"}", HTTPD_RESP_USE_STRLEN);
  }
  if (activeInstance_->configRepo_ != nullptr) {
    esp_err_t err = activeInstance_->configRepo_->FactoryReset();
    if (err != ESP_OK) {
      httpd_resp_set_status(request, "500 Internal Server Error");
      return httpd_resp_send(request, "{\"error\":\"reset_failed\"}", HTTPD_RESP_USE_STRLEN);
    }
    // Reset in-memory limits to defaults
    activeInstance_->UpdateSensorLimits(100, 600, 50, 5000);
    activeInstance_->apSsidCode_ = 0;
  }
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request,
    "{\"reset\":true,\"message\":\"Configuracoes restauradas para fabrica. Reinicie o gateway se necessario.\"}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::BindingDiscoveryHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  return httpd_resp_send(request, "{\"error\":\"not_implemented\"}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::GroupPairingHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  return httpd_resp_send(request, "{\"error\":\"not_implemented\"}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::ModbusMasterTriggerHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  return httpd_resp_send(request, "{\"error\":\"not_implemented\"}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t NetworkManager::IoConfigHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

esp_err_t NetworkManager::IoOutputsHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

esp_err_t NetworkManager::IoPulseHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

esp_err_t NetworkManager::IoClearCountersHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

esp_err_t NetworkManager::IoMirrorDisarmHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

esp_err_t NetworkManager::OtaUploadHandler(httpd_req_t* request) {
  httpd_resp_set_status(request, "501 Not Implemented");
  httpd_resp_send(request, nullptr, 0);
  return ESP_OK;
}

bool NetworkManager::ExtractQueryValue(httpd_req_t* request, const char* key, uint16_t* value) const {
  if (request == nullptr || key == nullptr || value == nullptr) return false;
  const size_t queryLength = httpd_req_get_url_query_len(request);
  if (queryLength == 0) return false;
  char query[192] = {0};
  if (queryLength >= sizeof(query)) return false;
  if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK) return false;
  char rawValue[16] = {0};
  if (httpd_query_key_value(query, key, rawValue, sizeof(rawValue)) != ESP_OK) return false;
  *value = static_cast<uint16_t>(std::strtoul(rawValue, nullptr, 0));
  return true;
}

bool NetworkManager::ExtractSignedQueryValue(httpd_req_t* request, const char* key, int16_t* value) const {
  if (request == nullptr || key == nullptr || value == nullptr) return false;
  const size_t queryLength = httpd_req_get_url_query_len(request);
  if (queryLength == 0) return false;
  char query[192] = {0};
  if (queryLength >= sizeof(query)) return false;
  if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK) return false;
  char rawValue[16] = {0};
  if (httpd_query_key_value(query, key, rawValue, sizeof(rawValue)) != ESP_OK) return false;
  *value = static_cast<int16_t>(std::strtol(rawValue, nullptr, 0));
  return true;
}

bool NetworkManager::ExtractStringQueryValue(httpd_req_t* request, const char* key, char* value, size_t valueSize) const {
  if (request == nullptr || key == nullptr || value == nullptr || valueSize == 0) return false;
  const size_t queryLength = httpd_req_get_url_query_len(request);
  if (queryLength == 0) return false;
  char query[192] = {0};
  if (queryLength >= sizeof(query)) return false;
  if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK) return false;
  if (httpd_query_key_value(query, key, value, valueSize) != ESP_OK) return false;
  value[valueSize - 1] = '\0';
  return true;
}

bool NetworkManager::ReadFormBody(httpd_req_t* request, char* value, size_t valueSize) const {
  if (request == nullptr || value == nullptr || valueSize == 0) return false;
  if (request->content_len <= 0 || static_cast<size_t>(request->content_len) >= valueSize) return false;
  int totalRead = 0;
  while (totalRead < request->content_len) {
    const int read = httpd_req_recv(request, value + totalRead, request->content_len - totalRead);
    if (read <= 0) {
      value[0] = '\0';
      return false;
    }
    totalRead += read;
  }
  value[totalRead] = '\0';
  return true;
}

bool NetworkManager::ExtractFormStringValue(const char* formData, const char* key, char* value, size_t valueSize) const {
  if (formData == nullptr || key == nullptr || value == nullptr || valueSize == 0) return false;
  if (httpd_query_key_value(formData, key, value, valueSize) != ESP_OK) return false;
  value[valueSize - 1] = '\0';
  return true;
}

bool NetworkManager::ExtractFormValue(const char* formData, const char* key, uint16_t* value) const {
  if (value == nullptr) return false;
  char rawValue[16] = {0};
  if (!ExtractFormStringValue(formData, key, rawValue, sizeof(rawValue))) return false;
  *value = static_cast<uint16_t>(std::strtoul(rawValue, nullptr, 0));
  return true;
}

bool NetworkManager::ExtractFormValueU32(const char* formData, const char* key, uint32_t* value) const {
  if (value == nullptr) return false;
  char rawValue[16] = {0};
  if (!ExtractFormStringValue(formData, key, rawValue, sizeof(rawValue))) return false;
  *value = static_cast<uint32_t>(std::strtoul(rawValue, nullptr, 0));
  return true;
}

bool NetworkManager::ExtractSignedFormValue(const char* formData, const char* key, int16_t* value) const {
  if (value == nullptr) return false;
  char rawValue[16] = {0};
  if (!ExtractFormStringValue(formData, key, rawValue, sizeof(rawValue))) return false;
  *value = static_cast<int16_t>(std::strtol(rawValue, nullptr, 0));
  return true;
}

bool NetworkManager::IsAuthenticated(httpd_req_t* request) {
  if (request == nullptr || stateMutex_ == nullptr) return false;
  size_t cookieLength = httpd_req_get_hdr_value_len(request, "Cookie");
  if (cookieLength == 0 || cookieLength >= 128) return false;
  char cookie[128] = {0};
  if (httpd_req_get_hdr_value_str(request, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return false;
  const char* tokenStart = std::strstr(cookie, "pulse_session=");
  if (tokenStart == nullptr) return false;
  tokenStart += std::strlen("pulse_session=");
  char token[17] = {0};
  size_t index = 0;
  while (tokenStart[index] != '\0' && tokenStart[index] != ';' && index < (sizeof(token) - 1)) {
    token[index] = tokenStart[index];
    ++index;
  }
  token[index] = '\0';
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) != pdTRUE) return false;
  const int64_t nowUs = esp_timer_get_time();
  const bool tokenPresent = sessionToken_[0] != '\0';
  const bool notExpired = tokenPresent && sessionLastSeenUs_ != 0 && (nowUs - sessionLastSeenUs_) < kSessionTimeoutUs;
  const char* const stored = sessionToken_.data();
  const size_t tokenLen = sessionToken_.size() - 1;
  uint8_t diff = 0;
  for (size_t i = 0; i < tokenLen; ++i) {
    diff |= static_cast<uint8_t>(token[i] ^ stored[i]);
  }
  const bool valid = tokenPresent && notExpired && (diff == 0);
  if (valid) {
    sessionLastSeenUs_ = nowUs;
  } else if (tokenPresent && !notExpired) {
    ClearSession();
  }
  xSemaphoreGive(stateMutex_);
  return valid;
}

bool NetworkManager::ValidateCredentialValue(const char* value) const {
  if (value == nullptr || value[0] == '\0') return false;
  for (size_t index = 0; value[index] != '\0'; ++index) {
    const unsigned char current = static_cast<unsigned char>(value[index]);
    if (current < 33 || current > 126) return false;
  }
  return true;
}

void NetworkManager::ClearSession() {
  sessionToken_.fill('\0');
  sessionLastSeenUs_ = 0;
}

void NetworkManager::GenerateSessionToken() {
  static constexpr char kHex[] = "0123456789abcdef";
  const size_t limit = sessionToken_.size() - 1;
  for (size_t index = 0; index < limit;) {
    uint32_t randomValue = esp_random();
    for (uint8_t nibble = 0; nibble < 8 && index < limit; ++nibble, ++index) {
      sessionToken_[index] = kHex[(randomValue >> (nibble * 4u)) & 0x0Fu];
    }
  }
  sessionToken_[sessionToken_.size() - 1] = '\0';
}

bool NetworkManager::QueueHttpCommand(shared::AppCommandType type, uint16_t value) {
  if (commandQueue_ == nullptr) return false;
  shared::AppCommand command{};
  command.source = shared::AppCommandSource::kNetwork;
  command.type = type;
  command.value = value;
  command.auxiliary = 0;
  command.context = 0;
  command.detail = 0;
  command.flags = 0;
  return xQueueSend(commandQueue_, &command, 0) == pdTRUE;
}

esp_err_t NetworkManager::EnsureWifiStack() {
  if (wifiStackReady_) return ESP_OK;
  ESP_LOGI(kLogTag, "EnsureWifiStack: esp_netif_init...");
  esp_err_t err = esp_netif_init();
  ESP_LOGI(kLogTag, "EnsureWifiStack: esp_netif_init=%s", esp_err_to_name(err));
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
  err = esp_event_loop_create_default();
  ESP_LOGI(kLogTag, "EnsureWifiStack: event_loop=%s", esp_err_to_name(err));
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
  if (apNetif_ == nullptr) {
    apNetif_ = esp_netif_create_default_wifi_ap();
    if (apNetif_ == nullptr) { ESP_LOGE(kLogTag, "apNetif_ is NULL"); return ESP_FAIL; }
  }
  ESP_LOGI(kLogTag, "EnsureWifiStack: esp_wifi_init...");
  wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&wifiInitConfig);
  ESP_LOGI(kLogTag, "EnsureWifiStack: esp_wifi_init=%s", esp_err_to_name(err));
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) { ESP_LOGE(kLogTag, "set_storage failed: %s", esp_err_to_name(err)); return err; }
  err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
      &NetworkManager::HandleWifiEvent, nullptr, nullptr);
  ESP_LOGI(kLogTag, "EnsureWifiStack: handler_reg=%s", esp_err_to_name(err));
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
  wifiStackReady_ = true;
  ESP_LOGI(kLogTag, "EnsureWifiStack: DONE");
  return ESP_OK;
}

void NetworkManager::BuildApSsid(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) return;
  if (configSnapshot_.fixedApSsid[0] != '\0') {
    std::snprintf(buffer, bufferSize, "%s", configSnapshot_.fixedApSsid.data());
    return;
  }
  if (apSsidCode_ == 0) {
    std::snprintf(buffer, bufferSize, "SafeDock_xxxx");
    return;
  }
  std::snprintf(buffer, bufferSize, "SafeDock_%04u", static_cast<unsigned>(apSsidCode_ % 10000u));
}

void NetworkManager::BuildApIp(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) return;
  if (apNetif_ == nullptr) {
    std::snprintf(buffer, bufferSize, "0.0.0.0");
    return;
  }
  esp_netif_ip_info_t ipInfo{};
  if (esp_netif_get_ip_info(apNetif_, &ipInfo) != ESP_OK) {
    std::snprintf(buffer, bufferSize, "0.0.0.0");
    return;
  }
  std::snprintf(buffer, bufferSize, IPSTR, IP2STR(&ipInfo.ip));
}

void NetworkManager::BuildRemoteRuntimeText(char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) return;
  buffer[0] = '\0';
  size_t written = 0;
  for (size_t index = 0; index < shared::kMaxRxBufferEntries; ++index) {
    const auto& peer = remoteTelemetrySnapshot_[index];
    if (!peer.valid) continue;
    const int chunk = std::snprintf(
        buffer + written, bufferSize - written,
        "%sA%u:S%u:DI%u:DO%u",
        written == 0 ? "" : ";",
        peer.sourceAlias,
        static_cast<unsigned>(peer.systemState),
        peer.diDiscreteStateMask,
        peer.doStateMask);
    if (chunk <= 0 || static_cast<size_t>(chunk) >= (bufferSize - written)) break;
    written += static_cast<size_t>(chunk);
  }
}

esp_err_t NetworkManager::StartApMode() {
  // Lazy task creation — only runs when webserver is actually requested.
  if (taskHandle_ == nullptr) {
    BaseType_t taskCreated = xTaskCreate(&NetworkManager::TaskEntry, "network_task", kNetworkTaskStackSize, this, 3, &taskHandle_);
    if (taskCreated != pdPASS) {
      ESP_LOGE(kLogTag, "xTaskCreate falhou");
      return ESP_ERR_NO_MEM;
    }
  }

  ESP_LOGI(kLogTag, "StartApMode: EnsureWifiStack...");
  auto rollback_ap_start = [this]() {
    if (httpServer_ != nullptr) {
      httpd_stop(httpServer_);
      httpServer_ = nullptr;
    }
    if (wifiStackReady_) {
      esp_wifi_stop();
    }
    apStartedAtUs_ = 0;
    connectedStations_ = 0;
    ClearSession();
  };

  auto register_uri = [this](httpd_uri_t* uri) -> esp_err_t {
    return httpd_register_uri_handler(httpServer_, uri);
  };

  esp_err_t err = EnsureWifiStack();
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "EnsureWifiStack falhou: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(kLogTag, "StartApMode: WiFi stack OK, configurando AP...");

  wifi_config_t wifiConfig = {};
  // Reuse SSID code when always-on is enabled, otherwise generate new
  if (configSnapshot_.webServerAlwaysOn == 0 || apSsidCode_ == 0) {
    apSsidCode_ = static_cast<uint16_t>(esp_random() % 10000u);
  }
  BuildApSsid(reinterpret_cast<char*>(wifiConfig.ap.ssid), sizeof(wifiConfig.ap.ssid));
  wifiConfig.ap.ssid_len = static_cast<uint8_t>(std::strlen(reinterpret_cast<const char*>(wifiConfig.ap.ssid)));
  ESP_LOGI(kLogTag, "SSID='%.*s' len=%u", wifiConfig.ap.ssid_len, wifiConfig.ap.ssid, wifiConfig.ap.ssid_len);
  wifiConfig.ap.channel = kApChannel;
  wifiConfig.ap.max_connection = kApMaxConnections;
  const char* effectiveApPassword =
      configSnapshot_.fixedApPassword[0] != '\0' ? configSnapshot_.fixedApPassword.data() : kDefaultApPassword;
  std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.password), effectiveApPassword,
      sizeof(wifiConfig.ap.password) - 1);
  wifiConfig.ap.password[sizeof(wifiConfig.ap.password) - 1] = '\0';
  wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wifiConfig.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
  wifiConfig.ap.pmf_cfg.capable = true;
  wifiConfig.ap.pmf_cfg.required = false;
  wifiConfig.ap.ssid_hidden = 0;

  esp_netif_ip_info_t configuredIpInfo{};
  if (!BuildConfiguredApIpInfo(configSnapshot_, &configuredIpInfo)) {
    ESP_LOGE(kLogTag, "IP fixo do AP invalido");
    return ESP_ERR_INVALID_ARG;
  }

  err = esp_netif_dhcps_stop(apNetif_);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGE(kLogTag, "dhcps_stop() falhou: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_netif_set_ip_info(apNetif_, &configuredIpInfo);
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "set_ip_info() falhou: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_netif_dhcps_start(apNetif_);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
    ESP_LOGE(kLogTag, "dhcps_start() falhou: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "wifi_set_mode(AP) falhou: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_wifi_set_config(WIFI_IF_AP, &wifiConfig);
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "wifi_set_config(AP) falhou: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_wifi_start();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(kLogTag, "wifi_start() falhou: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(kLogTag, "StartApMode: WiFi iniciado, ligando HTTP server...");

  if (httpServer_ == nullptr) {
    httpd_config_t httpConfig = HTTPD_DEFAULT_CONFIG();
    httpConfig.server_port = 80;
    httpConfig.stack_size = kHttpServerStackSize;
    httpConfig.max_uri_handlers = 24;
    err = httpd_start(&httpServer_, &httpConfig);
    if (err != ESP_OK) {
      ESP_LOGE(kLogTag, "httpd_start() falhou: %s", esp_err_to_name(err));
      rollback_ap_start();
      return err;
    }

    httpd_uri_t rootUri = {.uri = "/", .method = HTTP_GET, .handler = &NetworkManager::RootHandler, .user_ctx = nullptr};
    httpd_uri_t paramsUri = {.uri = "/params", .method = HTTP_GET, .handler = &NetworkManager::ParametersPageHandler, .user_ctx = nullptr};
    httpd_uri_t statusUri = {.uri = "/api/status", .method = HTTP_GET, .handler = &NetworkManager::StatusHandler, .user_ctx = nullptr};
    httpd_uri_t authStatusUri = {.uri = "/api/auth/status", .method = HTTP_GET, .handler = &NetworkManager::AuthStatusHandler, .user_ctx = nullptr};
    httpd_uri_t loginUri = {.uri = "/api/login", .method = HTTP_POST, .handler = &NetworkManager::LoginHandler, .user_ctx = nullptr};
    httpd_uri_t logoutUri = {.uri = "/api/logout", .method = HTTP_POST, .handler = &NetworkManager::LogoutHandler, .user_ctx = nullptr};
    httpd_uri_t exitUri = {.uri = "/api/exit-ap", .method = HTTP_POST, .handler = &NetworkManager::ExitApHandler, .user_ctx = nullptr};
    httpd_uri_t deviceConfigUri = {.uri = "/api/device/config", .method = HTTP_POST, .handler = &NetworkManager::DeviceConfigHandler, .user_ctx = nullptr};
    httpd_uri_t otaUploadUri = {.uri = "/api/device/ota", .method = HTTP_POST, .handler = &NetworkManager::OtaUploadHandler, .user_ctx = nullptr};
    httpd_uri_t bindingDiscoveryUri = {.uri = "/api/device/binding-discovery", .method = HTTP_POST, .handler = &NetworkManager::BindingDiscoveryHandler, .user_ctx = nullptr};
    httpd_uri_t groupPairingUri = {.uri = "/api/device/group-pairing", .method = HTTP_POST, .handler = &NetworkManager::GroupPairingHandler, .user_ctx = nullptr};
    httpd_uri_t restartUri = {.uri = "/api/device/restart", .method = HTTP_POST, .handler = &NetworkManager::RestartHandler, .user_ctx = nullptr};
    httpd_uri_t factoryResetUri = {.uri = "/api/device/factory-reset", .method = HTTP_POST, .handler = &NetworkManager::FactoryResetHandler, .user_ctx = nullptr};
    httpd_uri_t ioConfigUri = {.uri = "/api/io/config", .method = HTTP_POST, .handler = &NetworkManager::IoConfigHandler, .user_ctx = nullptr};
    httpd_uri_t ioOutputsUri = {.uri = "/api/io/outputs", .method = HTTP_POST, .handler = &NetworkManager::IoOutputsHandler, .user_ctx = nullptr};
    httpd_uri_t ioPulseUri = {.uri = "/api/io/pulse", .method = HTTP_POST, .handler = &NetworkManager::IoPulseHandler, .user_ctx = nullptr};
    httpd_uri_t ioClearCountersUri = {.uri = "/api/io/clear-counters", .method = HTTP_POST, .handler = &NetworkManager::IoClearCountersHandler, .user_ctx = nullptr};
    httpd_uri_t ioMirrorDisarmUri = {.uri = "/api/io/mirror/disarm", .method = HTTP_POST, .handler = &NetworkManager::IoMirrorDisarmHandler, .user_ctx = nullptr};
    httpd_uri_t modbusMasterTriggerUri = {.uri = "/api/modbus-master/trigger", .method = HTTP_POST, .handler = &NetworkManager::ModbusMasterTriggerHandler, .user_ctx = nullptr};

    err = register_uri(&rootUri);
    if (err == ESP_OK) err = register_uri(&paramsUri);
    if (err == ESP_OK) err = register_uri(&statusUri);
    if (err == ESP_OK) err = register_uri(&authStatusUri);
    if (err == ESP_OK) err = register_uri(&loginUri);
    if (err == ESP_OK) err = register_uri(&logoutUri);
    if (err == ESP_OK) err = register_uri(&exitUri);
    if (err == ESP_OK) err = register_uri(&deviceConfigUri);
    if (err == ESP_OK) err = register_uri(&otaUploadUri);
    if (err == ESP_OK) err = register_uri(&bindingDiscoveryUri);
    if (err == ESP_OK) err = register_uri(&groupPairingUri);
    if (err == ESP_OK) err = register_uri(&restartUri);
    if (err == ESP_OK) err = register_uri(&factoryResetUri);
    if (err == ESP_OK) err = register_uri(&ioConfigUri);
    if (err == ESP_OK) err = register_uri(&ioOutputsUri);
    if (err == ESP_OK) err = register_uri(&ioPulseUri);
    if (err == ESP_OK) err = register_uri(&ioClearCountersUri);
    if (err == ESP_OK) err = register_uri(&ioMirrorDisarmUri);
    if (err == ESP_OK) err = register_uri(&modbusMasterTriggerUri);
    if (err != ESP_OK) {
      ESP_LOGE(kLogTag, "Falha ao registrar URI: %s", esp_err_to_name(err));
      rollback_ap_start();
      return err;
    }
  }

  apStartedAtUs_ = esp_timer_get_time();
  ESP_LOGI(kLogTag, "SoftAP configurado e webserver iniciado");
  return ESP_OK;
}

esp_err_t NetworkManager::StopApMode() {
  if (httpServer_ != nullptr) {
    httpd_stop(httpServer_);
    httpServer_ = nullptr;
  }
  if (wifiStackReady_) {
    esp_wifi_stop();
    esp_wifi_deinit();
    wifiStackReady_ = false;
  }
  if (apNetif_ != nullptr) {
    esp_netif_destroy_default_wifi(apNetif_);
    apNetif_ = nullptr;
  }
  apStartedAtUs_ = 0;
  connectedStations_ = 0;
  ClearSession();
  apModeActive_ = false;
  // Delete the network task when AP stops
  if (taskHandle_ != nullptr) {
    vTaskDelete(taskHandle_);
    taskHandle_ = nullptr;
  }
  ESP_LOGI(kLogTag, "SoftAP e webserver parados");
  return ESP_OK;
}

void NetworkManager::PublishEvent(shared::SystemEventId id, uint16_t argument) const {
  if (systemEventQueue_ == nullptr) return;
  shared::SystemEvent event;
  event.id = id;
  event.argument = argument;
  if (xQueueSend(systemEventQueue_, &event, 0) != pdTRUE) {
    ESP_LOGW(kLogTag, "Falha ao publicar evento id=%u arg=%u", static_cast<unsigned>(id), argument);
  }
}

void NetworkManager::Run() {
  PublishEvent(shared::SystemEventId::kNetworkReady, 0);
  NetworkRequest request;
  while (true) {
    lastHeartbeatMillis_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
    const bool hasRequest = xQueueReceive(requestQueue_, &request, pdMS_TO_TICKS(100)) == pdTRUE;
    bool alwaysOnEnabled = false;
    bool apModeActive = false;
    int64_t apStartedAtUs = 0;
    if (stateMutex_ != nullptr && xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(25)) == pdTRUE) {
      alwaysOnEnabled = configSnapshot_.webServerAlwaysOn != 0;
      apModeActive = apModeActive_;
      apStartedAtUs = apStartedAtUs_;
      xSemaphoreGive(stateMutex_);
    }
    if (alwaysOnEnabled && !apModeActive) {
      const esp_err_t startErr = SetApModeEnabled(true);
      if (startErr != ESP_OK) {
        ESP_LOGE(kLogTag, "Auto-start do AP falhou: %s", esp_err_to_name(startErr));
      }
    } else if (!alwaysOnEnabled && apModeActive && apStartedAtUs != 0 &&
               (esp_timer_get_time() - apStartedAtUs) >= ComputeApAutoStopUs(configSnapshot_)) {
      const esp_err_t stopErr = SetApModeEnabled(false);
      if (stopErr != ESP_OK) {
        ESP_LOGE(kLogTag, "Auto-stop do AP falhou: %s", esp_err_to_name(stopErr));
      }
    }
    if (!hasRequest) continue;
    esp_err_t commandErr = ESP_OK;
    switch (request.command.type) {
      case shared::AppCommandType::kEnterApMode:
        commandErr = SetApModeEnabled(true);
        break;
      case shared::AppCommandType::kExitApMode:
        commandErr = SetApModeEnabled(false);
        break;
      default:
        break;
    }
    if (commandErr != ESP_OK) {
      ESP_LOGE(kLogTag, "Comando falhou: %d", static_cast<int>(commandErr));
    }
  }
}

}  // namespace system
}  // namespace pulse
