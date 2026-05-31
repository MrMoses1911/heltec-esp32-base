#include "Arduino.h"
#include "platform/platform_bootstrap.h"
#include "sensors/q5x_pfm.h"
#include "ui/status_view.h"
#include "ui/menu_manager.h"
#include "storage/nvs_store.h"
#include "storage/config_repository.h"
#include "system/network_manager.h"
#include "shared/app_types.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <math.h>

using namespace pulse::ui;

Q5X_PFM g_sensor;
pulse::ui::MenuManager g_menu;
pulse::storage::NvsStore g_nvs;
pulse::storage::ConfigRepository g_configRepo(&g_nvs);
pulse::system::NetworkManager g_networkManager;

static unsigned long g_bootMs = 0;
static unsigned long g_lastDistanceDisplayMs = 0;
static unsigned long g_lastRenderMs = 0;
static unsigned long g_lastUptimeSecond = 0;
static uint32_t g_uptimeMinutes = 0;
static bool g_webServerActive = false;

void TryFactoryReset() {
  Serial.println("Executando factory reset...");
  if (g_webServerActive) {
    g_networkManager.SetApModeEnabled(false);
  }
  g_configRepo.FactoryReset();
  delay(100);
  ESP.restart();
}

static pulse::shared::DeviceConfig MakeDeviceConfig(const pulse::storage::SafeDockConfig& cfg) {
  pulse::shared::DeviceConfig dc;
  dc.webServerAlwaysOn = cfg.webServerAlwaysOn;
  dc.webServerTimeoutMinutes = cfg.webServerTimeoutMinutes;
  std::copy(cfg.apSsid, cfg.apSsid + 33, dc.fixedApSsid.begin());
  std::copy(cfg.apPassword, cfg.apPassword + 64, dc.fixedApPassword.begin());
  std::copy(cfg.webUsername, cfg.webUsername + 16, dc.webUsername.begin());
  std::copy(cfg.webPassword, cfg.webPassword + 16, dc.webPassword.begin());
  return dc;
}

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  pulse::platform::InitializeBoard();

  g_nvs.Initialize();

  pulse::storage::SafeDockConfig config;
  g_configRepo.Load(&config);
  g_menu.SetConfig(config);

  g_networkManager.Initialize(nullptr, MakeDeviceConfig(config));
  g_networkManager.SetConfigRepository(&g_configRepo);
  g_networkManager.UpdateSensorLimits(
    config.freqMinHz, config.freqMaxHz,
    config.distanceMinMm, config.distanceMaxMm);

  // ── Load geometry config and setup alarm GPIO ────────────
  uint16_t adj, alMin, alMax, alMode;
  int16_t off;
  g_configRepo.LoadGeoConfig(&adj, &off, &alMin, &alMax, &alMode);
  g_networkManager.UpdateGeometryData(0, 0, adj, off, alMin, alMax, alMode, false);

  gpio_config_t alarmCfg = {};
  alarmCfg.pin_bit_mask = 1ULL << config.alarmGpio;
  alarmCfg.mode = GPIO_MODE_OUTPUT;
  alarmCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  alarmCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&alarmCfg);
  gpio_set_level(static_cast<gpio_num_t>(config.alarmGpio), 0);
  Serial.printf("Alarme GPIO %d configurado\n", config.alarmGpio);

  // Auto-start webserver if always-on is enabled
  if (config.webServerAlwaysOn) {
    uint16_t savedCode = 0;
    g_configRepo.LoadSsidCode(&savedCode);
    if (savedCode != 0) {
      g_networkManager.SetApSsidCode(savedCode);
    }
    auto devCfg = MakeDeviceConfig(config);
    esp_err_t err = g_networkManager.SetApModeEnabled(true);
    if (err == ESP_OK) {
      g_networkManager.UpdateSnapshot(
        pulse::shared::SystemState::kApMode,
        devCfg,
        pulse::shared::IoSnapshot{},
        pulse::shared::LoRaRuntimeStatus{});
      g_webServerActive = true;
      g_menu.SetWebServerActive(true);
      Serial.println("Webserver auto-iniciado (always-on)");
    }
  }

  g_menu.NavigateTo(pulse::ui::Screen::kMainMenu);
  pulse::ui::ShowBootLogo();
  delay(4000);

  g_sensor.init();
  g_bootMs = millis();
}

void loop() {
  pulse::platform::PollBoard();
  unsigned long now = millis();

  if (g_menu.IsFactoryResetConfirmed()) {
    g_menu.ClearFactoryResetConfirmed();
    TryFactoryReset();
  }

  if (g_menu.IsWebServerRequested() && !g_webServerActive) {
    g_menu.ClearWebServerRequested();
    pulse::storage::SafeDockConfig cfg = g_menu.GetConfig();
    auto devCfg = MakeDeviceConfig(cfg);
    esp_err_t err = g_networkManager.SetApModeEnabled(true);
    if (err != ESP_OK) {
      Serial.printf("ERRO ao iniciar webserver: 0x%X (%s)\n", err, esp_err_to_name(err));
    } else {
      g_networkManager.UpdateSnapshot(
        pulse::shared::SystemState::kApMode,
        devCfg,
        pulse::shared::IoSnapshot{},
        pulse::shared::LoRaRuntimeStatus{});
      g_webServerActive = true;
      g_menu.SetWebServerActive(true);
      Serial.println("Webserver iniciado");
    }
  }

  if (g_menu.IsWebServerStopRequested() && g_webServerActive) {
    g_menu.ClearWebServerStopRequested();
    g_networkManager.SetApModeEnabled(false);
    g_webServerActive = false;
    g_menu.SetWebServerActive(false);
    Serial.println("Webserver parado");
  }

  g_sensor.update();

  // ── Pythagoras: vehicle distance + alarm GPIO ────────────
  float rawDist = g_sensor.getDistanceMm();
  float freq = g_sensor.getFrequencyHz();
  bool valid = (g_sensor.getStatus() == Q5X_PFM::kOk);
  float vehicleDist = 0;
  uint16_t adjMm = g_networkManager.GetAdjacentMm();
  int16_t offMm  = g_networkManager.GetOffsetMm();
  uint16_t alMin = g_networkManager.GetAlarmMinMm();
  uint16_t alMax = g_networkManager.GetAlarmMaxMm();
  uint16_t alMode = g_networkManager.GetAlarmMode();

  if (valid && adjMm > 0) {
    float h2 = rawDist * rawDist;
    float a2 = static_cast<float>(adjMm) * adjMm;
    if (h2 > a2) {
      vehicleDist = sqrtf(h2 - a2);
    } else {
      vehicleDist = 0;
    }
  } else {
    vehicleDist = valid ? rawDist : 0;
  }
  vehicleDist += static_cast<float>(offMm);
  if (vehicleDist < 0) vehicleDist = 0;

  bool alarmActive = (alMode == 0)
    ? (vehicleDist >= alMin && vehicleDist <= alMax)
    : (vehicleDist < alMin || vehicleDist > alMax);
  gpio_set_level(static_cast<gpio_num_t>(34), alarmActive ? 1 : 0);

  if (g_webServerActive) {
    g_networkManager.UpdateSensorData(rawDist, freq, valid);
    g_networkManager.UpdateRuntimeData(vehicleDist, rawDist, alarmActive);
  }

  if (now - g_lastUptimeSecond >= 60000) {
    g_lastUptimeSecond = now;
    g_uptimeMinutes = (now - g_bootMs) / 60000;
  }

  if (now - g_lastDistanceDisplayMs >= 250) {
    g_lastDistanceDisplayMs = now;
    g_menu.SetDistanceReading(rawDist, freq, valid);
  }

  if (pulse::platform::ConsumeButtonSingleClick()) {
    g_menu.HandleButtonClick();
    g_lastRenderMs = 0;
  }

  if (pulse::platform::ConsumeButtonLongPress(2000)) {
    g_menu.HandleButtonLongPress();
    g_lastRenderMs = 0;
    pulse::platform::BlinkConfirmationLed();
  }

  Screen currentScreen = g_menu.CurrentScreen();

  if (currentScreen == Screen::kDistanceView) {
    if (now - g_lastRenderMs >= 250) {
      g_lastRenderMs = now;
      pulse::ui::RenderStatus(g_menu);
    }
  } else {
    if (now - g_lastRenderMs >= 500) {
      g_lastRenderMs = now;
      pulse::ui::RenderStatus(g_menu);
    }
  }

  delay(10);
}
