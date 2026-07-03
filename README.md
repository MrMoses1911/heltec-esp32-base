# heltec-esp32-base

Template limpo para projetos com **Heltec WiFi LoRa V3** (ESP32-S3 + SX1262) no **ESP-IDF 5.5.3** com framework **Arduino**.

Baseado na arquitetura do Pulse SafeDock, removendo a lógica de aplicação e mantendo o esqueleto reutilizável: inicialização da placa, display OLED, botão, LED e rádio LoRa.

## Requisitos

- ESP-IDF 5.5.3 instalado (`C:\esp\v5.5.3\esp-idf`)
- Toolchain ESP32-S3 (`C:\Espressif\tools\`)
- Python venv configurado pelo `export.bat`

Este repositório vive no WSL, mas os comandos IDF executam no Windows porque as ferramentas são Windows-native.

## Quick Start

```batch
:: cmd.exe do Windows
C:\esp\v5.5.3\esp-idf\export.bat
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

Na primeira vez, o `build.ninja` é gerado pelo CMake. Depois disso, builds incrementais podem rodar do WSL:

```bash
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build
```

Para reconfigurar ou flash, volte ao Windows.

## Estrutura

```text
CMakeLists.txt                    # Projeto ESP-IDF
sdkconfig.defaults                # Configurações tracked (8MB, Arduino, OTA)
partition_tables/
  partitions_8mb_factory_ota_balanced.csv
components/
  heltec_esp32_lora_v3/           # Placa: OLED, botão, LED, rádio
  arduino/                        # Arduino core para ESP32
  RadioLib/                       # Driver SX1262
  esp8266-oled-ssd1306/           # Display OLED via I2C
  HotButton/                      # Botão com debounce
main/
  main.cpp                        # smoke-test funcional
  CMakeLists.txt
  platform/
    platform_bootstrap.h          # API de abstração da placa
    platform_bootstrap.cpp        # Implementação Heltec
```

## Camada De Abstração

O código de aplicação nunca chama `heltec_unofficial.h` diretamente. Tudo passa por `pulse::platform::`:

| Categoria | Funções |
|---|---|
| Placa | `InitializeBoard()`, `PollBoard()` |
| LED | `BlinkConfirmationLed()` |
| Botão | `IsButtonPressed()`, `ConsumeButtonSingleClick()`, `ConsumeButtonLongPress(ms)` |
| OLED | `DisplayClear()`, `DisplaySetFontNormal()`, `DisplaySetFontLarge()`, `DisplayDrawText()`, `DisplayMeasureTextWidth()`, `DisplayDrawImage()`, `DisplayDrawPageImage()`, `DisplayPresent()` |
| LoRa | `InitializeRadio()`, `ConfigureRadio(freq,bw,sf,power)`, `SetRadioReceiveAction()`, `ClearRadioReceiveAction()`, `StartRadioReceive()`, `TransmitRadio()`, `ReadRadioData()`, `GetRadioRssi()`, `GetRadioSnr()` |

## Firmware Inicial

O `main/main.cpp` já vem como smoke-test funcional: inicializa OLED, configura o SX1262 em RX contínuo, mostra status no display, pisca o LED no click simples e reinicia o rádio com long press de 2s.

Para iniciar uma aplicação real, mantenha `pulse::platform::InitializeBoard()` no `setup()`, chame `pulse::platform::PollBoard()` no `loop()` e substitua o restante pela lógica do seu produto.

## Configurações

O arquivo `sdkconfig` é gerado pelo menuconfig. Para persistir alterações:

```batch
idf.py menuconfig
idf.py save-defconfig
```

Isso atualiza `sdkconfig.defaults`.

Padrões atuais:

- Flash: 8MB, DIO, 80MHz
- Partição: factory + ota_0 + ota_1 (`0x290000` cada)
- Arduino framework: core 1, loop stack 8192 bytes
- FreeRTOS tick: 1000 Hz
- Cache: 16KB instrução + 32KB dados, 8-way, line 32B

## OTA

A tabela de partições reserva três slots de 2,5MB (factory + 2 OTA). Para atualização OTA, use `esp_https_ota` ou `esp_ota_ops`.

## Componentes Incluídos

| Componente | Origem |
|---|---|
| `heltec_esp32_lora_v3` | Heltec oficial |
| `arduino` | ESP32 Arduino core |
| `RadioLib` | jgromes/RadioLib |
| `esp8266-oled-ssd1306` | ThingPulse |
| `HotButton` | Rop Gonggrijp / Heltec |
