#include "Arduino.h"
#include "platform/platform_bootstrap.h"
#include "sensors/q5x_pfm.h"
#include "ui/status_view.h"
#include "ui/menu_manager.h"
#include "storage/nvs_store.h"
#include "storage/config_repository.h"
#include "system/network_manager.h"
#include "shared/app_types.h"

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
  pulse::platform::InitializeBoard();

  g_nvs.Initialize();

  pulse::storage::SafeDockConfig config;
  g_configRepo.Load(&config);
  g_menu.SetConfig(config);

  g_networkManager.Initialize(nullptr, MakeDeviceConfig(config));

  g_menu.NavigateTo(pulse::ui::Screen::kMainMenu);
  pulse::ui::ShowBootLogo();
  delay(1500);

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
    g_networkManager.SetApModeEnabled(true);
    g_networkManager.UpdateSnapshot(
      pulse::shared::SystemState::kApMode,
      devCfg,
      pulse::shared::IoSnapshot{},
      pulse::shared::LoRaRuntimeStatus{});
    g_webServerActive = true;
    Serial.println("Webserver iniciado");
  }

  g_sensor.update();

  if (now - g_lastUptimeSecond >= 60000) {
    g_lastUptimeSecond = now;
    g_uptimeMinutes = (now - g_bootMs) / 60000;
  }

  if (now - g_lastDistanceDisplayMs >= 250) {
    g_lastDistanceDisplayMs = now;
    float dist = g_sensor.getDistanceMm();
    float freq = g_sensor.getFrequencyHz();
    auto status = g_sensor.getStatus();
    g_menu.SetDistanceReading(dist, freq, status == Q5X_PFM::kOk);
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
