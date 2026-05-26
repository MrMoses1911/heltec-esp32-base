#ifndef PULSE_SAFEDOCK_UI_MENU_MANAGER_H
#define PULSE_SAFEDOCK_UI_MENU_MANAGER_H

#include <cstdint>
#include <array>
#include "storage/config_repository.h"

namespace pulse {
namespace ui {

enum class Screen : uint8_t {
  kMainMenu = 0,
  kDistanceView,
  kWebServerConfirm,
  kWebServerMenu,
  kWebMonitor,
  kWebRuntime,
  kWebSystem,
  kWebSensor,
  kFactoryResetConfirm,
};

enum class DistanceMode : uint8_t {
  kFiltered = 0,
  kRaw = 1,
};

class MenuManager {
 public:
  MenuManager();

  void Reset();
  void HandleButtonClick();
  void HandleButtonLongPress();
  Screen CurrentScreen() const;
  uint8_t CurrentSelection() const;

  void SetConfig(const storage::SafeDockConfig& config);
  const storage::SafeDockConfig& GetConfig() const;
  void SetConfigValue(const storage::SafeDockConfig& config);

  bool IsFactoryResetConfirmed() const;
  void ClearFactoryResetConfirmed();

  void SetDistanceReading(float mm, float hz, bool valid);
  DistanceMode GetDistanceMode() const;
  void ToggleDistanceMode();

  struct DistanceData {
    float distanceMm = 0;
    float freqHz = 0;
    bool valid = false;
  };

  const DistanceData& GetDistanceData() const;

  void SetWebServerState(uint16_t alwaysOn, uint16_t timeoutMin, const char* ssid, const char* password, const char* username);

  struct WebServerData {
    uint16_t alwaysOn = 0;
    uint16_t timeoutMin = 15;
    char ssid[33] = {};
    char password[64] = {};
    char username[16] = {};
    uint32_t uptimeMinutes = 0;
  };

  const WebServerData& GetWebServerData() const;

  void SetRuntimeData(uint16_t alias, uint32_t uptimeMin, uint16_t rxPackets, uint16_t txPackets);
  struct RuntimeData {
    uint16_t deviceAlias = 1;
    uint32_t uptimeMinutes = 0;
    uint16_t rxPackets = 0;
    uint16_t txPackets = 0;
  };
  const RuntimeData& GetRuntimeData() const;

  void NavigateTo(Screen screen);
  bool IsWebServerRequested() const;
  void ClearWebServerRequested();
  bool IsWebServerStopRequested() const;
  void ClearWebServerStopRequested();
  void SetWebServerActive(bool active);
  bool IsWebServerActive() const;

 private:
  Screen currentScreen_;
  uint8_t selection_;
  bool factoryResetConfirmed_;
  bool webServerRequested_;
  bool webServerStopRequested_;
  bool webServerActive_;
  DistanceData distanceData_;
  DistanceMode distanceMode_;
  WebServerData webServerData_;
  RuntimeData runtimeData_;
  storage::SafeDockConfig config_;
};

}  // namespace ui
}  // namespace pulse

#endif
