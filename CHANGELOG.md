# Changelog

## [1.0.0] — 2026-05-16

### Adicionado

#### Esqueleto funcional (Sprint 1)
- `main/shared/`: Tipos compartilhados do Pulse IoT Gateway (app_types, app_commands, app_events, build_features, io_snapshot)
- `main/system/`: WebServer/AP skeleton com NetworkManager (stub) + catálogo de textos PT-BR para web UI
- `main/ui/`: OLED display skeleton com status_view, boot_logo, gateway_logo
- `main/comms/`: LoRa protocolo (v7) + LoRaManager skeleton
- `main/CMakeLists.txt`: Registro de todos os sources + REQUIRES
- `main/main.cpp`: Inicialização dos managers + display de distância periódico

#### Renomeação
- Projeto renomeado de `heltec-esp32-base` para `Pulse-SafeDock`
- CMakeLists.txt, .vscode/settings.json, README.md atualizados

#### Menu OLED + Botão + NVS (Sprint 2)
- `main/storage/`: NvsStore + ConfigRepository com SafeDockConfig (freq limits, webserver, credenciais)
- `main/ui/menu_manager.h/.cpp`: Máquina de estados com 8 telas, HotButton (click/long press)
- `main/ui/status_view.cpp` reescrito: MainMenu, DistanceView, WebServerMenu, Monitor, Runtime, System, Sensor, FactoryResetConfirm
- `main/main.cpp` reescrito: setup/loop com navegação por botão, factory reset via nvs_flash_erase

### Alterado
- `main/CMakeLists.txt`: +storage/*.cpp, +ui/menu_manager.cpp, +REQUIRES nvs_flash
- `AGENTS.md`: Atualizado com estrutura completa do projeto

### Técnico
- Build: ESP-IDF 5.5.3, target ESP32-S3, Arduino framework
- Partição: 8MB, factory + ota_0 + ota_1
- Placa: Heltec WiFi LoRa V3 (SX1262)
- Sensor: Banner Q5X laser PFM (GPIO 38)
