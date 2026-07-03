# Pulse SafeDock — Arquitetura

```
┌──────────────────────────────────────────────────────────┐
│                       main.cpp                           │
│                setup() / loop()                          │
├──────────┬──────────┬──────────┬──────────┬──────────────┤
│  comms/  │  ui/     │ system/  │ storage/ │  sensors/    │
│  LoRa    │  OLED    │ WebSrv   │ NVS      │  Q5X PFM     │
│  (stub)  │  Menu    │ HTTP AP  │ Config   │  PCNT-based  │
├──────────┴──────────┴──────────┴──────────┴──────────────┤
│              platform/platform_bootstrap                  │
│   (Heltec: OLED, button, radio, LED)                     │
├──────────────────────────────────────────────────────────┤
│               ESP-IDF 5.5.3 + Arduino                     │
│  (FreeRTOS, esp_http_server, nvs_flash, esp_wifi, PCNT)  │
└──────────────────────────────────────────────────────────┘
```

## Namespaces

| Namespace | Módulo | Função |
|---|---|---|
| `pulse::platform` | platform/ | Abstração de placa (OLED, botão, rádio) |
| `pulse::sensors` | sensors/ | Sensor Q5X PFM via PCNT |
| `pulse::shared` | shared/ | Tipos compartilhados (config, comandos, eventos) |
| `pulse::storage` | storage/ | Persistência NVS (nvs_store, config_repository) |
| `pulse::system` | system/ | WebServer, NetworkManager, HTTP handlers |
| `pulse::comms` | comms/ | LoRa (stub) |
| `pulse::ui` | ui/ | OLED display + MenuManager |

## Fluxo de Inicialização

```cpp
setup():
  1. Serial.begin + esp_log_level_set
  2. platform::InitializeBoard()
  3. g_nvs.Initialize()
  4. g_configRepo.Load(&config)       // blob principal
  5. g_networkManager.Initialize(...)
  6. g_configRepo.LoadGeoConfig(...)  // geometria + alarme
  7. gpio_config(alarmGpio=34, OUTPUT, LOW)
  8. if alwaysOn: LoadSsidCode + SetApModeEnabled(true)
  9. g_menu.NavigateTo(kMainMenu)
  10. ShowBootLogo()
  11. g_sensor.init()  // PCNT GPIO 38
```

## Fluxo Principal (loop)

```
loop():
  1. PollBoard()                    // heltec_loop (HotButton)
  2. Check factoryReset
  3. Check webserverRequest/Stop
  4. g_sensor.update()              // no-op (timer task faz tudo)
  5. PCNT timer (80ms): count→Hz→mm→EMA→distVeículo→alarme
  6. GPIO 34 = alarmActive
  7. Update web socket data
  8. Button: click → HandleButtonClick / long press → HandleButtonLongPress
  9. RenderStatus() conforme screen atual
  10. delay(10)
```

## PCNT — Medição

```
PCNT unit (GPIO 38) → rising edge counter
        ↓
esp_timer (80ms, ESP_TIMER_TASK)
        ↓
delta = count - lastCount
freqHz = delta / 0.08s
distMm = 50 + (freqHz - 100) * 9.9
        ↓
if PULSE_FEATURE_SENSOR_FILTER=1: EMA α=0.35, clamp 200mm
```

## Composição dos Components

```cmake
idf_component_register(SRCS "main.cpp"
    "comms/lora_manager.cpp" "comms/lora_protocol.cpp"
    "platform/platform_bootstrap.cpp"
    "sensors/q5x_pfm.cpp"
    "storage/config_repository.cpp" "storage/nvs_store.cpp"
    "system/network_manager.cpp"
    "ui/menu_manager.cpp" "ui/status_view.cpp"
    INCLUDE_DIRS "."
    REQUIRES app_update arduino esp_driver_pcnt esp_event
             esp_http_server esp_netif esp_wifi
             heltec_esp32_lora_v3 nvs_flash)
```

## Dependências Entre Módulos

```
main.cpp
  ├── platform/platform_bootstrap    (sempre)
  ├── sensors/q5x_pfm                (sempre)
  ├── ui/status_view                 (sempre via menu)
  ├── ui/menu_manager                (sempre)
  ├── storage/config_repository      (startup)
  ├── storage/nvs_store              (startup)
  ├── shared/app_types               (indireto)
  ├── system/network_manager         (webserver)
  ├── comms/lora_manager             (stub)
  └── comms/lora_protocol            (stub)
```
