# Pulse SafeDock — Firmware for Heltec WiFi LoRa V3

ESP-IDF 5.5.3 firmware targeting **ESP32-S3** / **Heltec WiFi LoRa V3** (SX1262). Reads a Banner Q5X laser sensor via PFM on GPIO 38 and displays distance on OLED with a button-navigated menu system.

> This repo runs in WSL but **all ESP-IDF commands must execute on the Windows host** (the toolchain lives under `C:\Espressif\tools\`, Python venv under `C:\esp\v5.5.3\esp-idf`).

---

## ⚠️ Always read before implementing

Before any implementation, read these files:

1. **[CHANGELOG.md](./CHANGELOG.md)** — what has already been built and what version we're on
2. **[docs/TECHNICAL_SPEC.md](./docs/TECHNICAL_SPEC.md)** — full technical specification for recreating the project
3. **[docs/RECAP.md](./docs/RECAP.md)** — step-by-step recap of every implementation
4. **[docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)** — architecture, namespaces, data flow

---

## Workflow rules

+ Toda mudança substancial exige **build de teste** antes de reportar conclusão.
+ Build sempre pelo Windows — nunca WSL.
+ Antes de implementar algo novo, leia CHANGELOG.md + TECHINICAL_SPEC.md para evitar duplicação.
+ Nunca apague ou renomeie arquivos sem perguntar ao usuário.

## Quick start (Windows)

```batch
:: cmd.exe
C:\esp\v5.5.3\esp-idf\export.bat
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

## Commands from WSL

First build always from Windows. Subsequent builds can run via Ninja:

```bash
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build
```

To reconfigure after CMakeLists.txt changes (must set toolchain manually):

```bash
cmd.exe /c "set PATH=C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;%PATH% && set IDF_PATH=C:\esp\v5.5.3\esp-idf && set PYTHON=C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe && C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -S C:\esp\Projetos\Pulse_SafeDock -B C:\esp\Projetos\Pulse_SafeDock\build -DCMAKE_TOOLCHAIN_FILE=C:\esp\v5.5.3\esp-idf\tools\cmake\toolchain-esp32s3.cmake -G Ninja"
```

## Project structure

```
main/
  CMakeLists.txt             10 source files, REQUIRES nvs_flash
  main.cpp                   setup() + loop() with menu system
  shared/                    Types (app_types, app_commands, app_events, build_features, io_snapshot)
  platform/                  Board abstraction (OLED, button, radio)
  sensors/                   Q5X PFM sensor
  ui/                        OLED + MenuManager (Screen state machine)
  storage/                   NVS persistence (nvs_store, config_repository)
  system/                    WebServer/AP (stub)
  comms/                     LoRa (stub)
```

## Full documentation

| File | Description |
|---|---|
| `docs/RECAP.md` | Complete implementation history |
| `docs/TECHNICAL_SPEC.md` | Full spec for recreating the project from scratch |
| `docs/ARCHITECTURE.md` | Architecture overview |
| `docs/INDEX.md` | Document index |
| `CHANGELOG.md` | Version changelog |

## Platform abstraction

`pulse::platform::` wraps Heltec board API (see `platform_bootstrap.h`):

| Category | Functions |
|---|---|
| Board | `InitializeBoard()`, `PollBoard()` |
| LED | `BlinkConfirmationLed()` |
| Button | `IsButtonPressed()`, `ConsumeButtonSingleClick()`, `ConsumeButtonLongPress(ms)` |
| OLED | `DisplayClear()`, `DisplaySetFontNormal()`, `DisplaySetFontLarge()`, `DisplayDrawText()`, `DisplayMeasureTextWidth()`, `DisplayDrawImage()`, `DisplayPresent()` |
| LoRa | `InitializeRadio()`, `ConfigureRadio(freq,bw,sf,power)`, `SetRadioReceiveAction()`, `StartRadioReceive()`, `TransmitRadio()`, `ReadRadioData()`, `GetRadioRssi()`, `GetRadioSnr()` |

## SDK config

`idf.py menuconfig` → `sdkconfig` (gitignored). Persist with `idf.py save-defconfig` → updates `sdkconfig.defaults` (tracked).

Defaults: 8MB flash DIO 80MHz, custom partition (factory + 2 OTA), Arduino framework, FreeRTOS tick 1000 Hz.

## Components

| Component | Purpose |
|---|---|
| `heltec_esp32_lora_v3` | Board: OLED, button, LED, SX1262 |
| `arduino` | Arduino framework |
| `RadioLib` | SX1262 driver |
| `esp8266-oled-ssd1306` | OLED via I2C |
| `HotButton` | Debounced button |
