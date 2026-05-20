#ifndef PULSE_SAFEDOCK_UI_STATUS_VIEW_H
#define PULSE_SAFEDOCK_UI_STATUS_VIEW_H

#include "esp_err.h"
#include "ui/menu_manager.h"
#include "storage/config_repository.h"

namespace pulse {
namespace ui {

void ShowBootLogo();
void ShowFatalError(esp_err_t err);

void RenderMainMenu(uint8_t selection);
void RenderDistanceView(float distanceMm, float freqHz, bool valid, DistanceMode mode);
void RenderWebServerConfirm(uint8_t selection);
void RenderWebServerMenu(uint8_t selection);
void RenderWebMonitor(const MenuManager::WebServerData& data);
void RenderWebRuntime(const MenuManager::RuntimeData& data);
void RenderWebSystem(uint8_t selection, const storage::SafeDockConfig& config);
void RenderWebSensor(uint8_t selection, const storage::SafeDockConfig& config);
void RenderFactoryResetConfirm(uint8_t selection);

void RenderStatus(const MenuManager& manager);

}  // namespace ui
}  // namespace pulse

#endif
