#include "ui/menu_manager.h"

namespace pulse {
namespace ui {

MenuManager::MenuManager()
  : currentScreen_(Screen::kMainMenu),
    selection_(0),
    factoryResetConfirmed_(false),
    webServerRequested_(false) {}

void MenuManager::Reset() {
  currentScreen_ = Screen::kMainMenu;
  selection_ = 0;
  factoryResetConfirmed_ = false;
  webServerRequested_ = false;
}

void MenuManager::HandleButtonClick() {
  switch (currentScreen_) {
    case Screen::kMainMenu:
      selection_ = (selection_ + 1) % 3;
      break;
    case Screen::kWebServerConfirm:
      selection_ = (selection_ + 1) % 2;
      break;
    case Screen::kWebServerMenu:
      selection_ = (selection_ + 1) % 4;
      break;
    case Screen::kFactoryResetConfirm:
      selection_ = (selection_ + 1) % 2;
      break;
    case Screen::kWebSystem:
    case Screen::kWebSensor:
      selection_ = (selection_ + 1) % 4;
      break;
    default:
      break;
  }
}

void MenuManager::HandleButtonLongPress() {
  switch (currentScreen_) {
    case Screen::kMainMenu:
      if (selection_ == 0) {
        currentScreen_ = Screen::kDistanceView;
      } else if (selection_ == 1) {
        currentScreen_ = Screen::kWebServerConfirm;
        selection_ = 0;
      } else if (selection_ == 2) {
        currentScreen_ = Screen::kFactoryResetConfirm;
        selection_ = 0;
      }
      break;

    case Screen::kDistanceView:
      currentScreen_ = Screen::kMainMenu;
      selection_ = 0;
      break;

    case Screen::kWebServerConfirm:
      if (selection_ == 0) {
        webServerRequested_ = true;
        currentScreen_ = Screen::kWebServerMenu;
        selection_ = 0;
      } else {
        currentScreen_ = Screen::kMainMenu;
        selection_ = 0;
      }
      break;

    case Screen::kWebServerMenu:
      if (selection_ == 0) currentScreen_ = Screen::kWebMonitor;
      else if (selection_ == 1) currentScreen_ = Screen::kWebRuntime;
      else if (selection_ == 2) currentScreen_ = Screen::kWebSystem;
      else if (selection_ == 3) currentScreen_ = Screen::kWebSensor;
      break;

    case Screen::kWebMonitor:
    case Screen::kWebRuntime:
      currentScreen_ = Screen::kWebServerMenu;
      selection_ = 0;
      break;

    case Screen::kWebSystem:
    case Screen::kWebSensor:
      currentScreen_ = Screen::kWebServerMenu;
      selection_ = 0;
      break;

    case Screen::kFactoryResetConfirm:
      if (selection_ == 0) {
        factoryResetConfirmed_ = true;
      }
      currentScreen_ = Screen::kMainMenu;
      selection_ = 0;
      break;
  }
}

Screen MenuManager::CurrentScreen() const {
  return currentScreen_;
}

uint8_t MenuManager::CurrentSelection() const {
  return selection_;
}

void MenuManager::SetConfig(const storage::SafeDockConfig& config) {
  config_ = config;
}

const storage::SafeDockConfig& MenuManager::GetConfig() const {
  return config_;
}

void MenuManager::SetConfigValue(const storage::SafeDockConfig& config) {
  config_ = config;
}

bool MenuManager::IsFactoryResetConfirmed() const {
  return factoryResetConfirmed_;
}

void MenuManager::ClearFactoryResetConfirmed() {
  factoryResetConfirmed_ = false;
}

bool MenuManager::IsWebServerRequested() const {
  return webServerRequested_;
}

void MenuManager::ClearWebServerRequested() {
  webServerRequested_ = false;
}

void MenuManager::SetDistanceReading(float mm, float hz, bool valid) {
  distanceData_.distanceMm = mm;
  distanceData_.freqHz = hz;
  distanceData_.valid = valid;
}

const MenuManager::DistanceData& MenuManager::GetDistanceData() const {
  return distanceData_;
}

void MenuManager::SetWebServerState(uint16_t alwaysOn, uint16_t timeoutMin, const char* ssid, const char* password, const char* username) {
  webServerData_.alwaysOn = alwaysOn;
  webServerData_.timeoutMin = timeoutMin;
  if (ssid) {
    size_t len = 0;
    while (len < sizeof(webServerData_.ssid) - 1 && ssid[len]) { webServerData_.ssid[len] = ssid[len]; ++len; }
    webServerData_.ssid[len] = '\0';
  }
  if (password) {
    size_t len = 0;
    while (len < sizeof(webServerData_.password) - 1 && password[len]) { webServerData_.password[len] = password[len]; ++len; }
    webServerData_.password[len] = '\0';
  }
  if (username) {
    size_t len = 0;
    while (len < sizeof(webServerData_.username) - 1 && username[len]) { webServerData_.username[len] = username[len]; ++len; }
    webServerData_.username[len] = '\0';
  }
}

const MenuManager::WebServerData& MenuManager::GetWebServerData() const {
  return webServerData_;
}

void MenuManager::SetRuntimeData(uint16_t alias, uint32_t uptimeMin, uint16_t rxPackets, uint16_t txPackets) {
  runtimeData_.deviceAlias = alias;
  runtimeData_.uptimeMinutes = uptimeMin;
  runtimeData_.rxPackets = rxPackets;
  runtimeData_.txPackets = txPackets;
}

const MenuManager::RuntimeData& MenuManager::GetRuntimeData() const {
  return runtimeData_;
}

void MenuManager::NavigateTo(Screen screen) {
  currentScreen_ = screen;
  selection_ = 0;
}

}  // namespace ui
}  // namespace pulse
