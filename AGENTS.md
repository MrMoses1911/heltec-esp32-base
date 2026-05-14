# heltec-esp32-base — Clean template for Heltec WiFi LoRa V3

ESP-IDF 5.5.3 firmware template targeting **ESP32-S3** / **Heltec WiFi LoRa V3** (SX1262). Based on the architecture of **Pulse IoT Gateway** but stripped of application-specific logic — reusable skeleton for new LoRa + OLED + Arduino projects.

> This repo runs in WSL but **all ESP-IDF commands must execute on the Windows host** (the toolchain lives under `C:\Espressif\tools\`, Python venv under `C:\esp\v5.5.3\esp-idf`).

## Quick start (Windows)

```batch
:: 1. Source the ESP-IDF environment (cmd.exe)
C:\esp\v5.5.3\esp-idf\export.bat

:: 2. Configure (first time or after adding components)
idf.py set-target esp32s3
idf.py reconfigure

:: 3. Build
idf.py build

:: 4. Flash + monitor
idf.py -p COM8 flash monitor
```

Port is typically `COM3`–`COM8` on Windows, `/dev/ttyACM0` or `/dev/ttyUSB0` on Linux.

## Commands from WSL

The first build (or any build after changing source files in `main/CMakeLists.txt`) must run from Windows to regenerate `build/build.ninja`:

```batch
C:\esp\v5.5.3\esp-idf\export.bat
idf.py build
```

After that, subsequent builds can run from WSL via Ninja directly:

```bash
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build
```

To reconfigure (`idf.py reconfigure`) or flash (`idf.py -p COM8 flash`), always switch to Windows cmd/PowerShell — do NOT source `export.sh` inside WSL (paths are Windows-native).

## Template structure

```
CMakeLists.txt               # Top-level project
sdkconfig.defaults            # Tracked defaults (8MB flash, Arduino framework, OEM partition)
partition_tables/
  partitions_8mb_factory_ota_balanced.csv  # factory + ota_0 + ota_1
components/
  heltec_esp32_lora_v3/       # Board abstraction (OLED, button, LED, SX1262)
  arduino/                    # ESP32 Arduino core
  RadioLib/                   # SX126x driver
  esp8266-oled-ssd1306/       # OLED display (SSD1306/SH1106)
  HotButton/                  # Button handling
main/
  main.cpp                    # Entry: Arduino setup() + loop()
  CMakeLists.txt              # Registers source files + REQUIRES components
  platform/
    platform_bootstrap.h      # Board abstraction header
    platform_bootstrap.cpp    # Heltec init, OLED, button, radio wrappers
```

## Platform abstraction

`main/platform/platform_bootstrap.h/.cpp` wraps the Heltec board API so application code never depends on `heltec_unofficial.h` directly. Available through `pulse::platform::`:

| Category | Functions |
|---|---|
| Board lifecycle | `InitializeBoard()`, `PollBoard()` |
| LED | `BlinkConfirmationLed()` |
| Button | `IsButtonPressed()`, `ConsumeButtonSingleClick()`, `ConsumeButtonLongPress(ms)` |
| OLED | `DisplayClear()`, `DisplayDrawText()`, `DisplayMeasureTextWidth()`, `DisplayDrawImage()`, `DisplayPresent()` |
| LoRa radio | `InitializeRadio()`, `ConfigureRadio(freq,bw,sf,power)`, `SetRadioReceiveAction()`, `StartRadioReceive()`, `TransmitRadio()`, `ReadRadioData()`, `GetRadioRssi()`, `GetRadioSnr()` |

## Entry point

`main/main.cpp` — Arduino-style `setup()` / `loop()`. The `loop()` stack is 8192 bytes. Board is initialised in `setup()` via `pulse::platform::InitializeBoard()`.

## SDK config

`idf.py menuconfig` changes go into `sdkconfig` (gitignored). To persist a change:

```batch
idf.py save-defconfig
```

This updates `sdkconfig.defaults` (tracked). Current defaults: 8MB flash (DIO 80MHz), custom partition table (factory + 2 OTA), Arduino framework enabled, ESP32-S3 cache defaults.

## Components overview

| Component | Source | Purpose |
|---|---|---|
| `heltec_esp32_lora_v3` | Heltec official | Board support: `heltec_unofficial.h`, `radio`, `display`, `button` globals |
| `arduino` | esp32-arduino | Arduino framework (`setup()`/`loop()`, `String`, `Serial`, etc.) |
| `RadioLib` | jgromes/RadioLib | SX1262 driver |
| `esp8266-oled-ssd1306` | ThingPulse/oled-ssd1306 | OLED display via I2C |
| `HotButton` | Heltec convenience | Debounced button state machine |

## Starting a new project

1. Copy this entire directory
2. Update `CMakeLists.txt` project name
3. Edit `main/main.cpp` — add your `setup()` / `loop()` logic
4. Add new source files to `main/CMakeLists.txt`
5. Use `pulse::platform::*` for board I/O (OLED, button, LoRa radio)

## OTA

Partition table reserves factory + 2 OTA slots (0x290000 each) on 8MB flash. For OTA updates, use `esp_https_ota` or `esp_ota_ops` (the `app_update` component is already pulled in by the toolchain).
