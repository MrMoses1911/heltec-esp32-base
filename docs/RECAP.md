# Pulse SafeDock — Histórico de Implementação

## v1.0.0 (FW 26.1.0.0)

### Sprint 1 — Esqueleto Funcional

- Tipos compartilhados do Pulse IoT Gateway (app_types, app_commands, app_events, build_features, io_snapshot)
- WebServer/AP skeleton com NetworkManager stub + catálogo de textos PT-BR
- OLED skeleton com boot_logo placeholder
- LoRa protocolo v7 + LoRaManager skeleton
- Projeto renomeado de `heltec-esp32-base` para `Pulse-SafeDock`
- CMakeLists.txt com registro de sources e REQUIRES

### Sprint 2 — Menu OLED + NVS

- `main/storage/`: NvsStore + ConfigRepository com SafeDockConfig
- `main/ui/menu_manager.h/.cpp`: máquina de estados com 8 telas
- HotButton: click simples avança, long press confirma
- NVS: ConfigRepository::Save/Load/FactoryReset
- status_view reescrito com MainMenu, DistanceView, WebServer submenus, FactoryResetConfirm
- main.cpp reescrito com navegação por botão

### Sprint 3 — WebServer Real + Correções

- NetworkManager real com Wi-Fi AP + HTTP server (esp_http_server)
- Páginas Monitor + Config com autenticação via session token
- JSON API /api/status com polling a cada 250ms
- Boot logo do IoT Gateway com DisplayDrawPageImage
- max_uri_handlers: 16 → 24 (corrige ESP_ERR_HTTPD_HANDLERS_FULL)
- ssid_hidden = 0 explícito
- Removido temperatura/umidade da interface
- Nomes de campo: dist_min_mm → distance_min_mm
- Defaults: sensor 90/660/40/2200 → 100/600/50/5000
- Network task: lazy creation

### Sprint 4 — Filtro de Sensor + PCNT

- ISR revertido para POSEDGE (ANYEDGE causava crash com Wi-Fi)
- Filtro: mediana 11 + MAD + EMA (α=0.12, clamp 50mm)
- PCNT periférico implementado (substitui GPIO ISR manual)
- Timer TASK de 80ms para amostragem
- Glitch filter por hardware (1µs)
- Watchpoint removido (causava crash — funções PCNT não IRAM-safe)
- `PULSE_FEATURE_SENSOR_FILTER` flag para raw/filtered

### Sprint 5 — Geometria + Alarme + Persistência

- Cálculo Pitágoras: distVeículo = sqrt(laser² - adjacente²) + offset
- GPIO 34 como saída de alarme
- Modos Zona (dentro) e Limite (fora)
- Configurações separadas por SaveGeoConfig/LoadGeoConfig
- NvsStore::ReadBlob aceita blobs menores
- ConfigRepository::Load fallback seguro
- ReadI16/WriteI16 adicionados
- Factory reset não reinicia ESP
- Web auto-save (onchange → triggerAutoSave → debounce 400ms)
- OTA via interface web

### Correções Gerais

- ExitApHandler envia resposta antes de parar HTTP server
- Restart/FactoryReset executam direto
- configSnapshot_ atualizado dentro do mutex
- AlarmMode_, AlarmMinMm_, AlarmMaxMm_ atualizados em activeInstance_
- DeviceConfigHandler: alMin/alMax/alMode inicializados de activeInstance_
- FactoryResetHandler: reseta credenciais + apSsid + sempre ligado
- JS: `!== undefined` em vez de `||` para valores 0
- Loop lê alarm config do NetworkManager via getters
- Testes unitários removidos
