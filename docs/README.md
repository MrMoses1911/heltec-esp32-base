# Pulse SafeDock

Template limpo para projetos com **Heltec WiFi LoRa V3** (ESP32-S3 + SX1262) no **ESP-IDF 5.5.3** com framework **Arduino**.

Baseado na arquitetura do [Pulse IoT Gateway](https://github.com/anomalyco/pulse-iot-gateway), removendo toda lógica de aplicação e mantendo apenas o esqueleto reutilizável: inicialização da placa, display OLED, botão, LED e rádio LoRa.

## Requisitos

- ESP-IDF 5.5.3 instalado (`C:\esp\v5.5.3\esp-idf`)
- Toolchain ESP32-S3 (`C:\Espressif\tools\`)
- Python venv configurado pelo `export.bat`

> Este repositório vive no WSL, mas **todos os comandos IDF executam no Windows** (as ferramentas são Windows-native).

## Quick start

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

```
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
  main.cpp                        # setup() + loop()
  CMakeLists.txt
  platform/
    platform_bootstrap.h          # API de abstração da placa
    platform_bootstrap.cpp        # Implementação Heltec
```

## Camada de abstração (`pulse::platform::`)

O código de aplicação nunca chama `heltec_unofficial.h` diretamente. Tudo passa por `pulse::platform::`:

| Categoria | Funções |
|---|---|
| Placa | `InitializeBoard()`, `PollBoard()` |
| LED | `BlinkConfirmationLed()` |
| Botão | `IsButtonPressed()`, `ConsumeButtonSingleClick()`, `ConsumeButtonLongPress(ms)` |
| OLED | `DisplayClear()`, `DisplayDrawText()`, `DisplayMeasureTextWidth()`, `DisplayDrawImage()`, `DisplayPresent()` |
| LoRa | `InitializeRadio()`, `ConfigureRadio(freq,bw,sf,power)`, `SetRadioReceiveAction()`, `StartRadioReceive()`, `TransmitRadio()`, `ReadRadioData()`, `GetRadioRssi()`, `GetRadioSnr()` |

## Como criar um novo projeto

1. Copie este diretório inteiro
2. Edite o nome em `CMakeLists.txt` (`project(meu-projeto)`)
3. Adicione seus sources em `main/CMakeLists.txt`
4. Escreva sua lógica em `main/main.cpp` (setup/loop)
5. Use `pulse::platform::*` para acessar a placa

```cpp
void setup() {
  Serial.begin(115200);
  pulse::platform::InitializeBoard();

  // Inicializa rádio
  pulse::platform::InitializeRadio();
  pulse::platform::ConfigureRadio(915.0, 500.0, 9, 10);
  pulse::platform::StartRadioReceive();
}

void loop() {
  pulse::platform::PollBoard();
  delay(10);
}
```

## Configurações (sdkconfig)

O arquivo `sdkconfig` (gitignored) é gerado pelo menuconfig. Para persistir alterações:

```batch
idf.py menuconfig
idf.py save-defconfig
```

Isso atualiza `sdkconfig.defaults` (tracked).

### Padrões atuais

- Flash: 8MB, DIO, 80MHz
- Partição: factory + ota_0 + ota_1 (0x290000 cada)
- Arduino framework: core 1, loop stack 8192 bytes
- FreeRTOS tick: 1000 Hz
- Cache: 16KB instrução + 32KB dados, 8-way, line 32B

## OTA

A tabela de partições reserva três slots de 2,5MB (factory + 2 OTA). Para atualização OTA, use `esp_https_ota` ou `esp_ota_ops`. O componente `app_update` já está disponível.

## Componentes incluídos

| Componente | Versão | Origem |
|---|---|---|
| heltec_esp32_lora_v3 | — | Heltec oficial |
| arduino | esp32-arduino | IDF component manager |
| RadioLib | 7.x | jgromes/RadioLib |
| esp8266-oled-ssd1306 | 4.x | ThingPulse |
| HotButton | — | Heltec |

## Licença

Este template é fornecido como referência. Os componentes third-party seguem suas próprias licenças.
