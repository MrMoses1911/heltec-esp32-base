# Pulse SafeDock

Sistema de monitoramento de doca para caminhões. Firmware para **Heltec WiFi LoRa V3** (ESP32-S3 + SX1262) que lê um sensor laser **Banner Q5X** via PFM, calcula a distância do veículo até a doca por geometria (Pitágoras), com alarme configurável e interface web completa.

**Versão FW:** 26.1.0.0 ✦ **Framework:** ESP-IDF 5.5.3 + Arduino

---

## Funcionalidades

- **Leitura laser**: Sensor PFM no GPIO 38 via PCNT (hardware pulse counter), sem interrupção manual
- **Geometria**: `distância do veículo = √(laser² − adjacente²) + offset`
- **Alarme**: GPIO 34, dois modos (zona ou limite), limites configuráveis
- **3 algoritmos de filtro**: EMA direto, Buffer+Mediana+EMA, Jump Detect+Mediana
- **Interface web**: Monitor em tempo real + Configurações com auto-save (sem botão "Salvar")
- **Atualização OTA**: Upload de firmware .bin pela interface web
- **Menu OLED**: Navegação por botão físico com distância, webserver e factory reset
- **Wi-Fi AP**: Sempre ligado por padrão, SSID personalizável
- **Persistência NVS**: Todos os parâmetros sobrevivem a reboot e flash

---

## Hardware

| Componente | Especificação | GPIO |
|---|---|---|
| Placa | Heltec WiFi LoRa V3 (ESP32-S3) | — |
| Sensor laser | Banner Q5X PFM (100-600Hz → 50-5000mm) | 38 (PCNT) |
| Alarme | Saída digital | 34 |
| Botão | Navegação OLED | 0 |
| Display | OLED SSD1306 128×64 (I2C) | — |

---

## Geometria da Doca

```
              Caminhão
                 /|
                / |
               /  |
              /   |
             /    |
            /     |
           /      |
          /       |
         /        |
        /         |
       /          |
      /           |
     /            |
    /             |
 Sensor          Doca
|------|----------------------|
```

- **laser** = distância medida pelo sensor (hipotenusa)
- **adjacente** = distância do sensor até a doca ao longo da parede
- **offset** = correção configurável (± mm)
- Se `adjacente = 0`: usa a leitura direta do sensor

---

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

Reconﬁgurar após mudanças no CMakeLists.txt:
```bash
cmd.exe /c "set PATH=C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;%PATH% && set IDF_PATH=C:\esp\v5.5.3\esp-idf && set PYTHON=C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe && C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -S C:\esp\Projetos\Pulse_SafeDock -B C:\esp\Projetos\Pulse_SafeDock\build -DCMAKE_TOOLCHAIN_FILE=C:\esp\v5.5.3\esp-idf\tools\cmake\toolchain-esp32s3.cmake -G Ninja"
```

---

## Credenciais Web

| Campo | Valor |
|---|---|
| Usuário | `admin` |
| Senha | `pulse` |
| IP AP | `192.168.10.1` |
| SSID (padrão) | `SafeDock_XXXX` |

---

## Algoritmos de Filtro do Sensor

| Modo | Nome | Algoritmo | Buffer | EMA α | Jump | Uso |
|---|---|---|---|---|---|---|
| 0 | **EMA direto** | EMA + clamp 200mm | — | 0.35 | — | Resposta crua, sem delay |
| 1 | **Suave** | Buffer 8 + mediana aparada + EMA | 640ms | 0.20 | — | Rejeita outliers, estável |
| 2 | **Jump Detect** | Buffer 6 + trim 1 + EMA | 480ms | 0.35 | 300mm | Salto → resposta instantânea |

---

## Modos de Alarme

| Modo | Gatilho | Exemplo |
|---|---|---|
| Zona | `min ≤ distância ≤ max` | Caminhão na posição de docagem |
| Limite | `distância < min ∨ distância > max` | Fora da zona segura |

---

## Estrutura do Projeto

```
main/
  CMakeLists.txt
  main.cpp
  shared/         Tipos (app_types, commands, events, build_features, io_snapshot)
  platform/       Abstração de placa (OLED, botão, rádio, LED)
  sensors/        Q5X PFM via PCNT + filtros
  ui/             OLED + MenuManager (máquina de estados)
  storage/        NVS (nvs_store, config_repository)
  system/         WebServer + Wi-Fi AP + handlers HTTP
  comms/          LoRa (stub)
```

---

## Documentação

| Arquivo | Descrição |
|---|---|
| `CHANGELOG.md` | Histórico de versões |
| `docs/TECHNICAL_SPEC.md` | Especificação técnica completa |
| `docs/ARCHITECTURE.md` | Arquitetura do firmware |
| `docs/RECAP.md` | Histórico de implementação |
