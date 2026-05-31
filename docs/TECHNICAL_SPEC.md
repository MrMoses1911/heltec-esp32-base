# Pulse SafeDock — Especificação Técnica

**Versão:** 1.0.0 — FW 26.1.0.0  
**Data:** 2026-05-31  
**Plataforma:** ESP-IDF 5.5.3 / ESP32-S3 / Heltec WiFi LoRa V3  

---

## 1. Requisitos de Ambiente

### 1.1 Windows (Toolchain)
- **ESP-IDF**: `C:\esp\v5.5.3\esp-idf\`
- **Toolchain**: `C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin\`
- **Python**: `C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe`
- **CMake**: `C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe`
- **Ninja**: `C:\Espressif\tools\ninja\1.12.1\ninja.EXE`

### 1.2 Build

```batch
:: Windows (cmd.exe) — primeira vez
C:\esp\v5.5.3\esp-idf\export.bat
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor

:: WSL — builds subsequentes
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build

:: Reconfigurar (após CMakeLists.txt)
cmd.exe /c "set PATH=C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;%PATH% && set IDF_PATH=C:\esp\v5.5.3\esp-idf && set PYTHON=C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe && C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -S C:\esp\Projetos\Pulse_SafeDock -B C:\esp\Projetos\Pulse_SafeDock\build -DCMAKE_TOOLCHAIN_FILE=C:\esp\v5.5.3\esp-idf\tools\cmake\toolchain-esp32s3.cmake -G Ninja"
```

---

## 2. Hardware

### 2.1 Placa: Heltec WiFi LoRa V3

| Recurso | Detalhe |
|---|---|
| MCU | ESP32-S3, dual-core Xtensa LX7, 160MHz |
| Flash | 8MB DIO 80MHz |
| PSRAM | 8MB Octal |
| LoRa | SX1262 (SPI) |
| OLED | SSD1306 128x64 (I2C) |
| Botão | GPIO 0 (BUTTON), pull-up interno |
| LED | GPIO 35 |

### 2.2 Pinagem do Projeto

| GPIO | Função | Tipo |
|---|---|---|
| 0 | Botão de usuário | Input, pull-up |
| 34 | Alarme (saída) | Output |
| 35 | LED built-in | Output |
| 38 | Sensor PFM (entrada) | Input (PCNT) |
| 39 | Alarme (reserva) | Output |

### 2.3 Sensor: Banner Q5X (PFM)

| Parâmetro | Valor |
|---|---|
| GPIO | 38 |
| Tipo | PFM (Pulse Frequency Modulation) |
| Faixa de frequência | 100-600 Hz |
| Faixa de distância | 50-5000 mm |
| Mapeamento | linear: 100Hz→50mm, 600Hz→5000mm |
| Slope | 9.9 mm/Hz |
| Filtro de glitch | 1µs (PCNT hardware) |

---

## 3. Geometria e Alarme

### 3.1 Cálculo da Distância do Veículo

```
distVeículo = sqrt(max(laser² - adjacente², 0)) + offset
```

Onde:
- `laser` = distância medida pelo sensor (hipotenusa)
- `adjacente` = distância configurada do sensor até a doca (mm)
- `offset` = correção configurável (+/- mm)
- Se `adjacente = 0`: `distVeículo = laser` (medição direta, sem Pitágoras)

### 3.2 Alarme

- GPIO 34: HIGH = alarme ativo
- Modo Zona: `alarme = (min ≤ distVeículo ≤ max)`
- Modo Limite: `alarme = (distVeículo < min ∨ distVeículo > max)`

---

## 4. PCNT — Medição de Frequência

O PCNT (Pulse Counter) periférico do ESP32-S3 conta bordas de subida no GPIO 38. Um `esp_timer` em modo TASK lê o contador a cada 80ms.

```
freqHz = deltaContagem / 0.080s
distMm = 50 + (freqHz - 100) * 9.9
```

- **Sem ISR**: o timer executa no task context do esp_timer, não em interrupção
- **Glitch filter**: rejeita pulsos < 1µs (configurado no PCNT)
- **Wrap handling**: `high_limit=32767`, delta negativo recebe +32769

---

## 5. Estrutura do Código

```
main/
  CMakeLists.txt            10 .cpp registrados
  main.cpp                  setup() + loop()

  shared/                   Tipos compartilhados (app_types, app_commands, app_events, build_features, io_snapshot)

  platform/                 Abstração de placa
    platform_bootstrap.h    API: InitializeBoard, Display*, Radio*
    platform_bootstrap.cpp  Implementação (Heltec, SSD1306, RadioLib)

  sensors/                  Sensor laser
    q5x_pfm.h/.cpp          PCNT-based frequency measurement, geometry calculation

  ui/                       OLED + Menu
    status_view.h/.cpp      Renderização de telas
    menu_manager.h/.cpp     Máquina de estados do menu

  storage/                  Persistência NVS
    nvs_store.h/.cpp        Wrapper nvs_flash (Read/Write U16, I16, Blob)
    config_repository.h/.cpp SafeDockConfig, SaveGeoConfig, LoadGeoConfig

  system/                   WebServer
    network_manager.h/.cpp  Wi-Fi AP + HTTP server + handlers
    web_ui_texts.h          Catálogo de textos PT-BR
    web_ui_texts/*.h        Textos (common, view, params)

  comms/                    LoRa (stub)
    lora_manager.h/.cpp
    lora_protocol.h/.cpp
```

---

## 6. Namespaces

| Namespace | Módulo |
|---|---|
| `pulse::platform` | Abstração de placa |
| `pulse::sensors` | Sensores |
| `pulse::shared` | Tipos compartilhados |
| `pulse::storage` | Persistência NVS |
| `pulse::system` | WebServer/Network |
| `pulse::comms` | LoRa |
| `pulse::ui` | OLED/Menu |

---

## 7. Web Interface

### 7.1 Endpoints

| Método | URL | Handler | Auth |
|---|---|---|---|
| GET | `/` | RootHandler | Não |
| GET | `/params` | ParametersPageHandler | Não |
| GET | `/api/status` | StatusHandler (JSON) | Não |
| GET | `/api/auth/status` | AuthStatusHandler | Não |
| POST | `/api/login` | LoginHandler | Não |
| POST | `/api/logout` | LogoutHandler | Sim |
| POST | `/api/device/config` | DeviceConfigHandler | Sim |
| POST | `/api/device/restart` | RestartHandler | Sim |
| POST | `/api/device/factory-reset` | FactoryResetHandler | Sim |
| POST | `/api/exit-ap` | ExitApHandler | Sim |
| POST | `/api/device/ota` | OtaUploadHandler | Sim |

### 7.2 Páginas

**Monitor** (`/`): Doca (distância veículo + alarme) → Sensor (distância/frequência) → WiFi → Runtime

**Configurações** (`/params`): Sensor (limites) → Doca/Geometria (adjacente, offset, alarme) → Rede → Sistema e Atualização (restart, factory reset, OTA)

### 7.3 Auto-Save

Toda alteração de campo dispara `triggerAutoSave()` → debounce 400ms → POST `/api/device/config`. Não há botão "Salvar tudo".

### 7.4 Credenciais Padrão

- Usuário: `admin`
- Senha: `pulse`

---

## 8. Persistência NVS

### 8.1 Namespace

Namespace: `safedock`

### 8.2 Chaves

| Chave | Tipo | Conteúdo |
|---|---|---|
| `config` | Blob | PersistedConfig: magic, schemaVersion, freqMin/Max Hz, distance Min/Max mm, sensorGpio, webUsername, webPassword, apSsid, apPassword, webServerAlwaysOn, webServerTimeoutMinutes |
| `ssid_code` | u16 | Código do SSID para always-on |
| `geo_adj` | u16 | Distância sensor→doca (mm) |
| `geo_offs` | i16 | Offset de correção |
| `geo_amin` | u16 | Alarme mínimo |
| `geo_amax` | u16 | Alarme máximo |
| `geo_mode` | u16 | Modo de alarme (0=zona, 1=limite) |

---

## 9. Firmware Version

Macros em `main/shared/build_features.h`:

| Macro | Valor |
|---|---|
| PULSE_FW_VERSION_YEAR | 26 |
| PULSE_FW_VERSION_MAJOR | 1 |
| PULSE_FW_VERSION_MINOR | 0 |
| PULSE_FW_VERSION_PATCH | 0 |

`PR: 26.1.0.0` via `pulse::shared::kFirmwareVersionLabel`

---

## 10. Build Features

Flag em `main/shared/build_features.h`:

| Macro | Default | Efeito |
|---|---|---|
| `PULSE_FEATURE_SENSOR_FILTER` | 1 | 1 = mediana+MAD+EMA; 0 = leitura crua |
| `PULSE_FEATURE_IO_MIRROR_REMOTE_DO_PLUS` | 0 | (reservado) |
| `PULSE_FEATURE_OTA_RELEASE_POLICY_STRICT` | 0 | (reservado) |

---

## 11. Dependências (components/)

| Componente | Função |
|---|---|
| `heltec_esp32_lora_v3` | Placa: OLED, botão, LED, SX1262 |
| `arduino` | Arduino framework |
| `RadioLib` | Driver SX1262 |
| `esp8266-oled-ssd1306` | OLED via I2C |
| `HotButton` | Botão com debounce |
