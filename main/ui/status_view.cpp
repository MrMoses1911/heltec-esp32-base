#include "ui/status_view.h"
#include "ui/boot_logo.h"
#include "platform/platform_bootstrap.h"
#include "shared/build_features.h"

#include <cstdio>
#include <cstring>

using namespace pulse::platform;

namespace pulse {
namespace ui {

static constexpr int kLineH = 12;
static constexpr int kTopY = 0;
static constexpr const char* kArrow = "> ";

static void DrawMenu(const char* title, const char* const* items, uint8_t itemCount, uint8_t selection) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, kTopY, title);

  int startY = kTopY + kLineH;
  for (uint8_t i = 0; i < itemCount; ++i) {
    int y = startY + i * kLineH;
    if (y + kLineH > 64) break;
    char buf[22];
    if (i == selection) {
      snprintf(buf, sizeof(buf), "%s%s", kArrow, items[i]);
    } else {
      snprintf(buf, sizeof(buf), "  %s", items[i]);
    }
    DisplayDrawText(0, y, buf);
  }
  DisplayPresent();
}

void ShowBootLogo() {
  DisplayClear();
  DisplayDrawPageImage(0, 0, kBootLogoWidth, kBootLogoHeight, logo_pulse_automa_o_compacto);
  DisplayPresent();
}

void ShowFatalError(esp_err_t err) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "ERRO FATAL");
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%X", err);
  DisplayDrawText(0, 16, buf);
  DisplayPresent();
}

// ── Main Menu ──────────────────────────────────────────────
void RenderMainMenu(uint8_t selection, bool webServerActive) {
  static const char* itemsOn[] = {
    "Distancia Laser",
    "Desligar Webserver",
    "Reset de Fabrica"
  };
  static const char* itemsOff[] = {
    "Distancia Laser",
    "Webserver",
    "Reset de Fabrica"
  };
  const char* const* items = webServerActive ? itemsOn : itemsOff;
  DrawMenu("Pulse SafeDock", items, 3, selection);
}

// ── Distance View ──────────────────────────────────────────
void RenderDistanceView(float distanceMm, float freqHz, bool valid, DistanceMode mode) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "Distancia Laser");

  const char* modeLabel = (mode == DistanceMode::kFiltered) ? "FILT" : "RAW ";
  DisplayDrawText(90, 0, modeLabel);

  if (valid) {
    DisplaySetFontLarge();
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f mm", static_cast<double>(distanceMm));
    DisplayDrawText(4, 18, buf);

    DisplaySetFontNormal();
    char buf2[16];
    snprintf(buf2, sizeof(buf2), "%.1f Hz", static_cast<double>(freqHz));
    DisplayDrawText(0, 40, buf2);
  } else {
    DisplaySetFontNormal();
    DisplayDrawText(0, 24, "SEM SINAL");
  }

  DisplaySetFontNormal();
  DisplayDrawText(0, 54, "Segure 2s p/ voltar");
  DisplayPresent();
}

// ── WebServer Confirm ──────────────────────────────────────
void RenderWebServerConfirm(uint8_t selection) {
  static const char* items[] = {
    "Sim",
    "Nao"
  };
  DrawMenu("Ligar Webserver?", items, 2, selection);
}

// ── WebServer Menu ─────────────────────────────────────────
void RenderWebServerMenu(uint8_t selection) {
  static const char* items[] = {
    "Monitor",
    "Runtime",
    "Sistema",
    "Sensor"
  };
  DrawMenu("Webserver", items, 4, selection);
}

// ── Web Monitor ────────────────────────────────────────────
void RenderWebMonitor(const MenuManager::WebServerData& data) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "Monitor");

  char buf[40];
  int y = kLineH;

  snprintf(buf, sizeof(buf), "SSID: %.16s", data.ssid[0] ? data.ssid : "Pulse-AP");
  DisplayDrawText(0, y, buf); y += kLineH;

  snprintf(buf, sizeof(buf), "Web: %s", data.alwaysOn ? "Sempre on" : "Temporizador");
  DisplayDrawText(0, y, buf); y += kLineH;

  snprintf(buf, sizeof(buf), "Timeout: %u min", data.timeoutMin);
  DisplayDrawText(0, y, buf); y += kLineH;

  snprintf(buf, sizeof(buf), "User: %.16s", data.username[0] ? data.username : "admin");
  DisplayDrawText(0, y, buf); y += kLineH;

  DisplayDrawText(0, 54, "Segure 2s para voltar");
  DisplayPresent();
}

// ── Web Runtime ────────────────────────────────────────────
void RenderWebRuntime(const MenuManager::RuntimeData& data) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "Runtime");

  char buf[22];
  int y = kLineH;

  snprintf(buf, sizeof(buf), "Alias: %u", data.deviceAlias);
  DisplayDrawText(0, y, buf); y += kLineH;

  snprintf(buf, sizeof(buf), "Up: %lu min", (unsigned long)data.uptimeMinutes);
  DisplayDrawText(0, y, buf); y += kLineH;

  snprintf(buf, sizeof(buf), "RX: %u  TX: %u", data.rxPackets, data.txPackets);
  DisplayDrawText(0, y, buf); y += kLineH;

  DisplayDrawText(0, y, pulse::shared::kFirmwareVersionLabel);
  y += kLineH;

  DisplayDrawText(0, 54, "Segure 2s para voltar");
  DisplayPresent();
}

// ── Web System Config ──────────────────────────────────────
void RenderWebSystem(uint8_t selection, const storage::SafeDockConfig& config) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "Sistema");

  static const char* labels[] = {
    "Sempre ligado",
    "Timeout (min)",
    "SSID",
    "User/Senha"
  };
  static const char* values[4];

  char v0[4], v1[8], v2[40], v3[40];
  snprintf(v0, sizeof(v0), "%s", config.webServerAlwaysOn ? "Sim" : "Nao");
  snprintf(v1, sizeof(v1), "%u", config.webServerTimeoutMinutes);
  snprintf(v2, sizeof(v2), "%.16s", config.apSsid[0] ? config.apSsid : "Pulse-AP");
  snprintf(v3, sizeof(v3), "%.16s / ****", config.webUsername[0] ? config.webUsername : "admin");
  values[0] = v0;
  values[1] = v1;
  values[2] = v2;
  values[3] = v3;

  int y = kLineH;
  for (uint8_t i = 0; i < 4; ++i) {
    char buf[24];
    if (i == selection) {
      snprintf(buf, sizeof(buf), ">%s: %s", labels[i], values[i]);
    } else {
      snprintf(buf, sizeof(buf), " %s: %s", labels[i], values[i]);
    }
    DisplayDrawText(0, y, buf);
    y += kLineH;
  }

  DisplayDrawText(0, 54, "Segure 2s para voltar");
  DisplayPresent();
}

// ── Web Sensor Config ─────────────────────────────────────
void RenderWebSensor(uint8_t selection, const storage::SafeDockConfig& config) {
  DisplayClear();
  DisplaySetFontNormal();
  DisplayDrawText(0, 0, "Sensor - Limites");

  static const char* labels[] = {
    "Freq. min (Hz)",
    "Freq. max (Hz)",
    "Dist. min (mm)",
    "Dist. max (mm)"
  };

  char v0[8], v1[8], v2[8], v3[8];
  snprintf(v0, sizeof(v0), "%u", config.freqMinHz);
  snprintf(v1, sizeof(v1), "%u", config.freqMaxHz);
  snprintf(v2, sizeof(v2), "%u", config.distanceMinMm);
  snprintf(v3, sizeof(v3), "%u", config.distanceMaxMm);

  const char* vals[] = { v0, v1, v2, v3 };

  int y = kLineH;
  for (uint8_t i = 0; i < 4; ++i) {
    char buf[24];
    if (i == selection) {
      snprintf(buf, sizeof(buf), ">%s %s", labels[i], vals[i]);
    } else {
      snprintf(buf, sizeof(buf), " %s %s", labels[i], vals[i]);
    }
    DisplayDrawText(0, y, buf);
    y += kLineH;
  }

  DisplayDrawText(0, 54, "Segure 2s para voltar");
  DisplayPresent();
}

// ── Factory Reset Confirm ─────────────────────────────────
void RenderFactoryResetConfirm(uint8_t selection) {
  static const char* items[] = {
    "Confirmar",
    "Cancelar"
  };
  DrawMenu("Reset de Fabrica", items, 2, selection);
}

// ── Main render dispatch ──────────────────────────────────
void RenderStatus(const MenuManager& manager) {
  Screen s = manager.CurrentScreen();
  uint8_t sel = manager.CurrentSelection();

  switch (s) {
    case Screen::kMainMenu:
      RenderMainMenu(sel, manager.IsWebServerActive());
      break;
    case Screen::kDistanceView: {
      auto& dd = manager.GetDistanceData();
      RenderDistanceView(dd.distanceMm, dd.freqHz, dd.valid, manager.GetDistanceMode());
      break;
    }
    case Screen::kWebServerConfirm:
      RenderWebServerConfirm(sel);
      break;
    case Screen::kWebServerMenu:
      RenderWebServerMenu(sel);
      break;
    case Screen::kWebMonitor:
      RenderWebMonitor(manager.GetWebServerData());
      break;
    case Screen::kWebRuntime:
      RenderWebRuntime(manager.GetRuntimeData());
      break;
    case Screen::kWebSystem:
      RenderWebSystem(sel, manager.GetConfig());
      break;
    case Screen::kWebSensor:
      RenderWebSensor(sel, manager.GetConfig());
      break;
    case Screen::kFactoryResetConfirm:
      RenderFactoryResetConfirm(sel);
      break;
  }
}

}  // namespace ui
}  // namespace pulse
