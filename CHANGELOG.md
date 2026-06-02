# Changelog

## [1.0.0] — 2026-05-31

### Sistema
- Firmware ESP-IDF 5.5.3 para Heltec WiFi LoRa V3 (ESP32-S3)
- Framework Arduino integrado
- Partição: 8MB, factory + ota_0 + ota_1 (2,5MB cada)
- Webserver always-on como padrão de fábrica
- Auto-start do AP na inicialização se always-on habilitado

### Sensor Laser (Q5X PFM)
- Leitura de frequência via PCNT periférico (hardware pulse counter)
- GPIO 38, 100-600Hz → 50-5000mm
- Timer TASK de 80ms para amostragem (sem ISR, sem crash com Wi-Fi)
- Filtro de glitch por hardware (PCNT, 1µs)
- Flag `PULSE_FEATURE_SENSOR_FILTER` para alternar entre leitura crua e filtrada
- 3 algoritmos de filtro selecionáveis via web:
  - **Modo 0 (EMA direto)**: EMA α=0.35 + clamp 200mm
  - **Modo 1 (Suave)**: Buffer 8 + mediana aparada (trima 25%) + EMA α=0.20
  - **Modo 2 (Jump Detect)**: Buffer 6 + trim 1 + EMA α=0.35 + Jump Detect 300mm (recomendado)
- Indicador do filtro ativo na página Monitor
- Persistido em NVS (`sens_mode`)

### Geometria — Distância do Veículo
- Cálculo: `distVeículo = sqrt(max(laser² - adjacente², 0)) + offset`
- Se adjacente = 0: `distVeículo = laser` (leitura direta)
- Offset de correção +/- configurável

### Alarme
- GPIO 34 como saída de alarme
- Modo Zona: alarme quando `min ≤ dist ≤ max`
- Modo Limite: alarme quando `dist < min ∨ dist > max`
- Limites min/max configuráveis na interface web
- Indicador na página Monitor com badge "ALARME ATIVO"

### Interface Web
- Página Monitor: Doca → Sensor → WiFi → Runtime
- Página Configurações (autenticada): sensor, geometria, alarme, rede, OTA
- Auto-save em qualquer alteração de campo (debounce 400ms)
- Atualização OTA via upload de arquivo .bin
- Factory reset (não reinicia o ESP)

### OLED + Botão
- Menu principal: Distância Laser, Webserver, Reset de Fábrica
- Click simples: navega/não faz/alterna modo de leitura
- Long press (2s): confirma/volta
- Boot logo do Pulse IoT Gateway (bitmap page-packed)
- Indicador FILT/RAW na tela de distância

### Persistência (NVS)
- Namespace `safedock` com blob de config + chaves separadas (geo, ssid)
- `NvsStore::ReadBlob` aceita blobs menores (compatibilidade retroativa)
- `ConfigRepository::Load` fallback seguro para defaults em qualquer erro
- `ReadI16/WriteI16` para valores negativos (offset)
- Configurações separadas por categoria: blob principal + SaveGeoConfig + SaveSsidCode

### Correções
- Network task lazy (criada sob demanda, deletada ao parar AP)
- max_uri_handlers: 24 (era 16, causava ESP_ERR_HTTPD_HANDLERS_FULL)
- ExitApHandler envia resposta antes de parar o HTTP server
- Restart/FactoryReset executam direto (sem fila que ninguém consumia)
- configSnapshot_ atualizado dentro do mutex no DeviceConfigHandler
- AlarmMode/AlarmMin/AlarmMax atualizados em memória + NVS
- GPIO 34 corrigido (era 39, depois 34 — input-only em ESP32, mas output no S3)
- JS usa `!== undefined` em vez de `||` para não tratar 0 como faltante
