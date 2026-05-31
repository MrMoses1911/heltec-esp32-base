# Pulse SafeDock

Firmware para **Heltec WiFi LoRa V3** (ESP32-S3 + SX1262) que lê um sensor laser **Banner Q5X** via PFM, calcula a distância de um veículo até a doca usando geometria (Pitágoras), e oferece interface web para monitoramento e configuração.

**Versão FW:** 26.1.0.0  
**Framework:** ESP-IDF 5.5.3 + Arduino  

## Funcionalidades

- Leitura de sensor PFM via PCNT periférico (hardware pulse counter, sem ISR)
- Cálculo da distância do veículo para a doca (hipotenusa → cateto oposto)
- Alarme configurável com saída GPIO 34 (modo zona ou limite)
- Interface web: monitor em tempo real + configurações com auto-save
- Atualização OTA via upload de firmware
- Menu OLED com botão físico (navegação + modos de leitura)
- Wi-Fi AP com SSID fixo ou personalizado, webserver always-on
- Persistência em NVS de todos os parâmetros

## Hardware

| Componente | Especificação |
|---|---|
| Placa | Heltec WiFi LoRa V3 (ESP32-S3) |
| Sensor | Banner Q5X PFM (100-600Hz → 50-5000mm) |
| GPIO sensor | 38 (PCNT) |
| GPIO alarme | 34 |
| Display | OLED SSD1306 128x64 (I2C) |

## Quick Start

```batch
:: Windows cmd.exe
C:\esp\v5.5.3\esp-idf\export.bat
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

Builds subsequentes via Ninja (WSL):
```bash
/mnt/c/Espressif/tools/ninja/1.12.1/ninja.EXE -C build
```

## Credenciais Web

- **Usuário:** `admin`
- **Senha:** `pulse`

## Documentação

| Arquivo | Descrição |
|---|---|
| `CHANGELOG.md` | Histórico de versões |
| `docs/TECHNICAL_SPEC.md` | Especificação técnica completa |
| `docs/ARCHITECTURE.md` | Arquitetura do firmware |
| `docs/RECAP.md` | Histórico de implementação |
