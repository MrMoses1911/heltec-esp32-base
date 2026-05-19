# Pulse SafeDock — Arquitetura

## Visão Geral

Firmware ESP-IDF 5.5.3 para **Heltec WiFi LoRa V3** (ESP32-S3 + SX1262). Lê sensor **Banner Q5X** via PFM no GPIO 38, exibe distância no OLED e oferece configuração via menu físico ou webserver.

```
┌─────────────────────────────────────────────────────┐
│                    main.cpp                         │
│              setup() / loop()                       │
├──────────┬──────────┬──────────┬──────────┬─────────┤
│  comms/  │  ui/     │ system/  │ storage/ │ sensors │
│  LoRa    │  OLED    │ WebSrv   │ NVS      │ Q5X     │
│  (stub)  │  Menu    │ (stub)   │ Config   │ PFM     │
├──────────┴──────────┴──────────┴──────────┴─────────┤
│              platform/platform_bootstrap             │
│   (Heltec board abstraction: OLED, button, radio)   │
├─────────────────────────────────────────────────────┤
│                 ESP-IDF 5.5.3 + Arduino              │
│     (FreeRTOS, esp_http_server, nvs_flash, Wi-Fi)    │
└─────────────────────────────────────────────────────┘
```

## Namespaces

| Namespace | Módulo |
|---|---|
| `pulse::platform` | Abstração de placa (OLED, botão, LED, rádio) |
| `pulse::sensors` | Sensores (Q5X PFM) |
| `pulse::shared` | Tipos compartilhados (config, comandos, eventos) |
| `pulse::storage` | Persistência NVS |
| `pulse::system` | WebServer, NetworkManager |
| `pulse::comms` | LoRa (protocolo + manager) |
| `pulse::ui` | OLED display, navegação por menu |

## Padrões de Design

- **Managers**: Todo subsistema tem um manager (singleton ou global) com `Initialize()` + métodos de ação
- **Filas**: Managers se comunicam via FreeRTOS queues (`QueueHandle_t`)
- **Eventos**: `SystemEvent` carrega `SystemEventId` + `uint16_t argument` para notificações coarse
- **Comandos**: `AppCommand` envelope padronizado entre subsistemas
- **Snapshot**: Estado runtime consolidado em structs (`IoSnapshot`, `LoRaRuntimeStatus`, etc.) para UI/web
- **Namespace `pulse::platform`**: Isola dependência de hardware (Heltec, RadioLib, SSD1306)

## Fluxo de Inicialização

```cpp
setup():
  1. Serial.begin(115200)
  2. pulse::platform::InitializeBoard()
  3. g_nvs.Initialize()
  4. g_configRepo.Load(&config)
  5. g_menu.SetConfig(config)
  6. pulse::ui::ShowBootLogo()
  7. g_sensor.init()
  8. g_bootMs = millis()
```

## Fluxo Principal (loop)

```
loop():
  1. PollBoard()                         ← heltec_loop (HotButton)
  2. Checa factoryResetConfirmed
  3. g_sensor.update()                   ← atualiza leitura laser
  4. Se 250ms: atualiza DistanceData no menu
  5. Se button singleClick: menu.HandleButtonClick()
  6. Se button longPress(2000): menu.HandleButtonLongPress() + BlinkConfirmationLed()
  7. RenderStatus(menu) conforme screen atual
```

## Dependências Entre Módulos

```
main.cpp
  ├── platform/platform_bootstrap   (sempre)
  ├── sensors/q5x_pfm               (sempre)
  ├── ui/status_view                 (sempre via menu)
  ├── ui/menu_manager                (sempre)
  ├── storage/config_repository      (setup)
  ├── storage/nvs_store              (setup)
  ├── shared/app_types               (setup indireto)
  ├── comms/lora_manager             (setup, stubs)
  └── system/network_manager         (setup, stubs)
```
