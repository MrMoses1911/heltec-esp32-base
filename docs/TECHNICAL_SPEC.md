# Pulse SafeDock — Descritivo Técnico

> Versão: 1.0.0  
> Data: 2026-05-16  
> Plataforma: ESP-IDF 5.5.3 / ESP32-S3 / Heltec WiFi LoRa V3  
> Projeto: `Pulse-SafeDock` (nome no CMake)

---

## 1. Requisitos de Ambiente

### 1.1 Windows (Host do Toolchain)

- **ESP-IDF**: `C:\esp\v5.5.3\esp-idf\`
- **Toolchain**: `C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin\`
- **Python venv**: `C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe`
- **CMake**: `C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe`
- **Ninja**: `C:\Espressif\tools\ninja\1.12.1\ninja.EXE`

### 1.2 WSL (Editor)

O repositório vive em `/mnt/c/esp/Projetos/Pulse_SafeDock`.  
Build deve ser via cmd.exe do Windows.

### 1.3 Build

```batch
:: Primeira vez (Windows cmd.exe)
C:\esp\v5.5.3\esp-idf\export.bat
idf.py set-target esp32s3
idf.py build

:: Builds subsequentes (WSL ou Windows)
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build

:: Para reconfigurar após alterar CMakeLists.txt
cmd.exe /c "set PATH=C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;%%PATH%% && set IDF_PATH=C:\esp\v5.5.3\esp-idf && set PYTHON=C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe && C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -S C:\esp\Projetos\Pulse_SafeDock -B C:\esp\Projetos\Pulse_SafeDock\build -DCMAKE_TOOLCHAIN_FILE=C:\esp\v5.5.3\esp-idf\tools\cmake\toolchain-esp32s3.cmake -G Ninja"

:: Flash
idf.py -p COM8 flash monitor
```

---

## 2. Configuração do Hardware

### 2.1 Placa: Heltec WiFi LoRa V3

| Recurso | Detalhe |
|---|---|
| MCU | ESP32-S3 (dual-core Xtensa LX7) |
| Flash | 8MB DIO 80MHz |
| PSRAM | 8MB Octal |
| LoRa | SX1262 (SPI) |
| OLED | SSD1306 128x64 (I2C) |
| Botão | GPIO 0 (BUTTON), pull-up interno |
| LED | GPIO 35 (built-in) |

### 2.2 Sensor: Banner Q5X Laser PFM

| Parâmetro | Valor |
|---|---|
| GPIO | 38 (entrada) |
| Tipo | PFM (Pulse Frequency Modulation) |
| Faixa de frequência | 90 Hz (2200 mm) a 660 Hz (40 mm) |
| Período mínimo | 1515 µs |
| Período máximo | 11111 µs |
| Filtro | Rejeita saltos >200 mm |
| Buffer | 7 períodos, saída por mediana |

### 2.3 Partição (8MB)

| Partição | Offset | Tamanho |
|---|---|---|
| nvs | 0x9000 | 128 KB |
| otadata | 0x29000 | 8 KB |
| phy_init | 0x2B000 | 4 KB |
| factory | 0x30000 | 2624 KB |
| ota_0 | 0x2C0000 | 2624 KB |
| ota_1 | 0x550000 | 2624 KB |

### 2.4 SDK Defaults (sdkconfig.defaults)

- Flash: 8MB DIO 80MHz
- Partição: custom (acima)
- Arduino: habilitado, core 1, loop stack 8192
- FreeRTOS tick: 1000 Hz
- Cache: 16KB instrução + 32KB dados, 8-way, line 32B
- Task WDT: habilitado

---

## 3. Estrutura do Código

### 3.1 Árvore Completa

```
main/
  CMakeLists.txt              ← Component registration (10 .cpp, REQUIRES)
  main.cpp                    ← setup() + loop()

  shared/                     ← Tipos compartilhados (sem .cpp, apenas headers)
    app_types.h               ← DeviceConfig, IoSnapshot, LoRaRuntimeStatus, enums
    app_commands.h            ← AppCommand envelope
    app_events.h              ← SystemEvent envelope
    build_features.h          ← Firmware version macros
    io_snapshot.h             ← IoSnapshot struct

  platform/                   ← Board abstraction
    platform_bootstrap.h      ← API (InitializeBoard, Display*, radio*)
    platform_bootstrap.cpp    ← Implementação (heltec_unofficial.h, RadioLib, SSD1306)

  sensors/                    ← Sensor drivers
    q5x_pfm.h                 ← Q5X_PFM class
    q5x_pfm.cpp               ← ISR period measurement, median filter

  sensors/                    ← Sensor drivers
    q5x_pfm.h/.cpp            ← Q5X PFM (GPIO38)
                              ← ISR on rising edge, 7-period buffer, median output

  ui/                         ← OLED display
    status_view.h/.cpp        ← RenderMainMenu, RenderDistanceView, RenderWebServer*, RenderFactoryResetConfirm
    menu_manager.h/.cpp       ← Screen state machine + navigation logic
    boot_logo.h               ← 128x64 bitmap placeholders
    gateway_logo.h

  storage/                    ← NVS persistence
    nvs_store.h/.cpp          ← nvs_flash wrapper (Initialize, Read/Write, EraseAll)
    config_repository.h/.cpp  ← SafeDockConfig save/load/factory-reset

  system/                     ← WebServer (stub)
    network_manager.h/.cpp    ← NetworkManager class (all methods return ESP_OK/false)
    web_ui_texts.h            ← Web UI text catalog macro assembly
    web_ui_texts/common.h     ← Common UI texts (PT-BR)
    web_ui_texts/view.h       ← Monitoring page texts
    web_ui_texts/params.h     ← Configuration page texts

  comms/                      ← LoRa (stub)
    lora_manager.h/.cpp       ← LoRaManager (queue/consume methods all return false)
    lora_protocol.h/.cpp      ← Protocol v7 (header serializer works, payloads return false)
```

### 3.2 CMakeLists.txt (main/)

```cmake
idf_component_register(SRCS "main.cpp"
                "comms/lora_manager.cpp"
                "comms/lora_protocol.cpp"
                "platform/platform_bootstrap.cpp"
                "sensors/q5x_pfm.cpp"
                "storage/config_repository.cpp"
                "storage/nvs_store.cpp"
                "system/network_manager.cpp"
                "ui/menu_manager.cpp"
                "ui/status_view.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino esp_event esp_http_server esp_netif esp_wifi heltec_esp32_lora_v3 nvs_flash)
```

Nota: `INCLUDE_DIRS "."` permite includes relativos como `#include "shared/app_types.h"`.

### 3.3 Dependências (components/)

| Componente | Diretório | Função |
|---|---|---|
| `heltec_esp32_lora_v3` | `components/heltec_esp32_lora_v3/` | Board: OLED, botão, LED, SX1262 |
| `arduino` | `components/arduino/` | Arduino framework (ESP32 core) |
| `RadioLib` | `components/RadioLib/` | SX1262 driver |
| `esp8266-oled-ssd1306` | `components/esp8266-oled-ssd1306/` | OLED via I2C |
| `HotButton` | `components/HotButton/` | Botão com debounce |

---

## 4. Namespaces e APIs

### 4.1 `pulse::platform` — Abstração de Placa

```cpp
void InitializeBoard();                                    // Serial, HSPI, OLED init
void PollBoard();                                          // heltec_loop()
void BlinkConfirmationLed();                                // 3x pisca LED

bool IsButtonPressed();                                    // Leitura direta do GPIO
bool ConsumeButtonSingleClick();                           // HotButton.isSingleClick()
bool ConsumeButtonLongPress(uint32_t durationMs);          // HotButton.pressedFor(ms)

// OLED
void DisplayClear();
void DisplaySetFontNormal();                               // ArialMT_Plain_10
void DisplaySetFontLarge();                                // ArialMT_Plain_16
void DisplayDrawText(int16_t x, int16_t y, const char* text);
int16_t DisplayMeasureTextWidth(const char* text);
void DisplayDrawImage(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t* image);
void DisplayPresent();

// LoRa Radio
int InitializeRadio();
int ConfigureRadio(float freqMhz, float bwKhz, uint8_t sf, int8_t powerDbm);
void SetRadioReceiveAction(void (*func)(void));
void ClearRadioReceiveAction();
int StartRadioReceive();
int TransmitRadio(const uint8_t* data, size_t length);
size_t GetRadioPacketLength();
int ReadRadioData(uint8_t* data, size_t length);
float GetRadioRssi();
float GetRadioSnr();
```

### 4.2 `pulse::sensors` — Q5X PFM

```cpp
class Q5X_PFM {
  void init(uint8_t gpio = 38);
  void update();                      // Call every loop iteration
  float getFrequencyHz();             // Current frequency
  float getDistanceMm();              // Converted distance
  enum Status { kOk, kNoSignal, kInvalid, kNotInitialized };
  Status getStatus();

  // Constants
  static constexpr float kFreqMinHz = 90.0f;
  static constexpr float kFreqMaxHz = 660.0f;
  static constexpr float kDistanceMinMm = 40.0f;
  static constexpr float kDistanceMaxMm = 2200.0f;
};
```

### 4.3 `pulse::storage` — Persistência

```cpp
// NvsStore — raw NVS wrapper
class NvsStore {
  esp_err_t Initialize();
  esp_err_t ReadU16(const char* ns, const char* key, uint16_t* v) const;
  esp_err_t WriteU16(const char* ns, const char* key, uint16_t v) const;
  esp_err_t ReadBlob(const char* ns, const char* key, void* v, size_t sz) const;
  esp_err_t WriteBlob(const char* ns, const char* key, const void* v, size_t sz) const;
  esp_err_t EraseAll();
};

// SafeDockConfig — persisted settings
struct SafeDockConfig {
  uint16_t schemaVersion = 1;
  uint16_t freqMinHz = 90;
  uint16_t freqMaxHz = 660;
  uint16_t distanceMinMm = 40;
  uint16_t distanceMaxMm = 2200;
  uint16_t sensorGpio = 38;
  char webUsername[16] = "admin";
  char webPassword[16] = "pulse";
  char apSsid[33] = {};
  char apPassword[64] = {};
  uint16_t webServerAlwaysOn = 0;
  uint16_t webServerTimeoutMinutes = 15;
};

// ConfigRepository — save/load/factory-reset
class ConfigRepository {
  explicit ConfigRepository(NvsStore* store);
  esp_err_t Load(SafeDockConfig* config);
  esp_err_t Save(const SafeDockConfig& config);
  esp_err_t FactoryReset();  // nvs_flash_erase()
};
```

### 4.4 `pulse::ui` — Menu OLED

```cpp
// Screen IDs
enum class Screen : uint8_t {
  kMainMenu, kDistanceView, kWebServerMenu,
  kWebMonitor, kWebRuntime, kWebSystem, kWebSensor,
  kFactoryResetConfirm
};

// MenuManager — state machine + navigation
class MenuManager {
  void Reset();
  void HandleButtonClick();          // Avança seleção
  void HandleButtonLongPress();      // Confirma/volta
  Screen CurrentScreen() const;
  uint8_t CurrentSelection() const;
  void SetConfig(const SafeDockConfig& config);
  void SetDistanceReading(float mm, float hz, bool valid);
  void SetWebServerState(...);
  void SetRuntimeData(uint16_t alias, uint32_t uptimeMin, uint16_t rx, uint16_t tx);
  bool IsFactoryResetConfirmed() const;
};

// Display renderers (status_view.h)
void RenderStatus(const MenuManager& manager);     // Dispatch central
void ShowBootLogo();
void ShowFatalError(esp_err_t err);
```

### 4.5 `pulse::system` — WebServer (Stub)

```cpp
class NetworkManager {
  esp_err_t Initialize(QueueHandle_t eventQueue, const DeviceConfig& config);
  esp_err_t SetApModeEnabled(bool enabled);
  bool QueueCommand(const AppCommand& cmd);
  bool ConsumeCommand(AppCommand* cmd);
  void UpdateSnapshot(SystemState st, const DeviceConfig& cfg, const IoSnapshot& io, const LoRaRuntimeStatus& lora);
  uint32_t LastHeartbeatMillis() const;
  // +20 HTTP handlers (static), all stubs returning 501
  // All implementation methods return ESP_OK / false
};
```

### 4.6 `pulse::comms` — LoRa (Stub)

```cpp
// lora_protocol: 10 message types, all payload structs, serializers/deserializers
// lora_manager: queue/consume pattern for 9 frame types
class LoRaManager {
  esp_err_t Initialize(QueueHandle_t eventQueue, const DeviceConfig& config);
  bool QueueBasicTelemetry(const BasicTelemetryPayload& p, uint16_t dest = kLoRaBroadcastAlias);
  bool QueueOutputCommand(...);
  // +7 Queue methods
  bool ConsumeTelemetry(ReceivedTelemetryFrame* f);
  // +8 Consume methods
  bool GetRuntimeStatus(LoRaRuntimeStatus* status) const;
  // All actual implementations return false / ESP_OK (stubs)
};
```

---

## 5. Menu Navigation State Machine

### 5.1 Telas

```
┌──────────────────────────┐
│       Main Menu          │
│  > Distancia Laser       │── long press ──► kDistanceView
│    Webserver             │── long press ──► kWebServerMenu
│    Reset de Fabrica      │── long press ──► kFactoryResetConfirm
└──────────────────────────┘

┌──────────────────────────┐
│    WebServer Menu        │
│  > Monitor               │── long press ──► kWebMonitor
│    Runtime               │── long press ──► kWebRuntime
│    Sistema               │── long press ──► kWebSystem
│    Sensor                │── long press ──► kWebSensor
└──────────────────────────┘

┌──────────────────────────┐
│  Reset de Fabrica        │
│  > Confirmar             │── long press ──► nvs_flash_erase + restart
│    Cancelar              │── long press ──► volta MainMenu
└──────────────────────────┘
```

### 5.2 Regras de Botão

- **Click simples**: `g_menu.HandleButtonClick()` → incrementa `selection_` (wrap-around)
- **Long press (2s)**: `g_menu.HandleButtonLongPress()` → navega entre telas ou confirma
- **Telas de exibição** (kDistanceView, kWebMonitor, kWebRuntime): long press volta ao menu pai
- **Telas de configuração** (kWebSystem, kWebSensor): long press volta ao WebServerMenu

---

## 6. Persistência NVS

### 6.1 Namespace

Toda config usa o namespace `"safedock"` no NVS.

### 6.2 Blob de Config

- Chave: `"config"`
- Struct binária com magic `0x53444643` ("SDFC")
- Schema version: 1

### 6.3 Factory Reset

```
ConfigRepository::FactoryReset()
  → nvs_store_.EraseAll()    // nvs_flash_erase()
  → main.cpp detecta e chama ESP.restart()
  → Na reinicialização, Load() retorna ESP_ERR_NVS_NOT_FOUND → defaults aplicados
```

---

## 7. Dependências ESP-IDF

```
REQUIRES: arduino, esp_event, esp_http_server, esp_netif, esp_wifi,
          heltec_esp32_lora_v3, nvs_flash
```

## 8. Portas e Pinagem

| GPIO | Função | Nota |
|---|---|---|
| 0 | BUTTON | Pull-up interno, active low |
| 35 | LED | Built-in |
| 38 | SENSOR_IN | Q5X PFM input, channel 8 |
| SPI (HSPI) | SX1262 | SCK, MISO, MOSI, SS via `heltec_unofficial.h` |
| I2C | SSD1306 | Via `heltec_unofficial.h` |

## 9. Firmware Version

Definido em `main/shared/build_features.h`:

| Macro | Padrão |
|---|---|
| PULSE_FW_VERSION_YEAR | 26 |
| PULSE_FW_VERSION_MAJOR | 1 |
| PULSE_FW_VERSION_MINOR | 0 |
| PULSE_FW_VERSION_PATCH | 0 |

Resultado: `"PR: 26.1.0.0"` via `pulse::shared::kFirmwareVersionLabel`.
