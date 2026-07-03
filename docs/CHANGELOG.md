# Changelog

## [1.1.0] — 2026-05-17

### Adicionado

- **Testes unitários**: `main/test/` com 4 suites (config, menu, sensor, network) usando framework Unity do ESP-IDF
  - `test_config.cpp`: ConfigRepository save/load roundtrip, SSID code persist, factory reset defaults
  - `test_menu.cpp`: MenuManager state machine — navegação cíclica, long press transitions, webserver confirm, factory reset
  - `test_sensor.cpp`: Constantes de frequência/distância, conversão freq→dist, limites de período
  - `test_network.cpp`: SSID code get/set, construtor

### Corrigido

- **Interface web**: StatusHandler retornava limites do sensor HARDCODED (100,600,50,5000) ignorando valores salvos no NVS
  - Adicionado `UpdateSensorLimits()` + 4 campos `sensorFreqMinHz_` etc. no NetworkManager
  - StatusHandler agora lê valores reais salvos
  - DeviceConfigHandler chama `UpdateSensorLimits()` após salvar no NVS
  - `main.cpp` carrega limites no `setup()` via `UpdateSensorLimits()`
- **Factory reset**: Não reinicia mais o ESP — apenas limpa NVS e reseta limites em memória
- **Sempre ligado**: `StartApMode` só gera SSID novo se `webServerAlwaysOn == 0`
- **SSID fixo**: `apSsidCode` persistido em NVS separadamente via `SaveSsidCode()`
- **Auto-start**: `setup()` inicia webserver automaticamente se `webServerAlwaysOn == 1`
- **ExitApHandler**: Resposta enviada ANTES de parar o HTTP server
- **Restart/FactoryReset/Binding/Group/Modbus**: Removido `QueueHttpCommand` (ninguém processava), execução direta

### Alterado

- `main/CMakeLists.txt`: Conditional test sources (+`unity` REQUIRES) quando `CONFIG_UNITY_ENABLE=y`

## [1.0.0] — 2026-05-16

### Adicionado

- Esqueleto funcional completo (shared, system, ui, comms)
- Menu OLED com botão (click + long press 2s)
- Filtro de sensor: mediana 11 + MAD + EMA (α=0.12, clamp 50mm)
- NVS persistence (nvs_store + config_repository)
- WebServer real com Wi-Fi AP (página Monitor + Config com login)
- Sensor laser Q5X PFM (GPIO 38, 100-600Hz, 50-5000mm)

### Corrigido

- ISR revertido para POSEDGE (removido ANYEDGE que crashava durante Wi-Fi)
- `max_uri_handlers`: 16 → 24
- Rede oculta: `ssid_hidden = 0` explícito
- Removido temperatura/umidade da interface web
- Nomes de campo: `dist_min_mm` → `distance_min_mm`
- Refresh rate: 3000ms → 250ms
- Defaults: sensor 90/660/40/2200 → 100/600/50/5000
- Network task: lazy creation (só quando webserver inicia)
