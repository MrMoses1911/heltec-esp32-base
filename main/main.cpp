#include "Arduino.h"
#include "platform/platform_bootstrap.h"

#include <cstdio>

namespace {

constexpr float kDefaultLoRaFrequencyMhz = 915.0F;
constexpr float kDefaultLoRaBandwidthKhz = 125.0F;
constexpr uint8_t kDefaultLoRaSpreadingFactor = 7;
constexpr int8_t kDefaultLoRaOutputPowerDbm = 10;
constexpr unsigned long kDisplayRefreshIntervalMs = 1000UL;

bool g_radioReady = false;
int g_radioState = 0;
unsigned long g_lastDisplayRefreshMs = 0;

bool ConfigureDefaultRadio() {
  int state = pulse::platform::InitializeRadio();
  if (state != 0) {
    g_radioState = state;
    return false;
  }

  state = pulse::platform::ConfigureRadio(
    kDefaultLoRaFrequencyMhz,
    kDefaultLoRaBandwidthKhz,
    kDefaultLoRaSpreadingFactor,
    kDefaultLoRaOutputPowerDbm);
  if (state != 0) {
    g_radioState = state;
    return false;
  }

  state = pulse::platform::StartRadioReceive();
  g_radioState = state;
  return state == 0;
}

void RenderStatus(const char* message) {
  char line[32] = {};

  pulse::platform::DisplayClear();
  pulse::platform::DisplaySetFontNormal();
  pulse::platform::DisplayDrawText(0, 0, "Heltec LoRa V3");
  pulse::platform::DisplayDrawText(0, 12, message == nullptr ? "Ready" : message);

  if (g_radioReady) {
    std::snprintf(line, sizeof(line), "LoRa RX %.1f MHz", kDefaultLoRaFrequencyMhz);
  } else {
    std::snprintf(line, sizeof(line), "LoRa error %d", g_radioState);
  }
  pulse::platform::DisplayDrawText(0, 24, line);

  std::snprintf(line, sizeof(line), "Uptime %lus", millis() / 1000UL);
  pulse::platform::DisplayDrawText(0, 36, line);
  pulse::platform::DisplayDrawText(0, 48, "Click: LED | Hold: radio");
  pulse::platform::DisplayPresent();
}

}  // namespace

void setup() {
  pulse::platform::InitializeBoard();
  Serial.println("Heltec WiFi LoRa V3 ready");

  RenderStatus("Starting...");
  g_radioReady = ConfigureDefaultRadio();
  if (g_radioReady) {
    Serial.println("LoRa radio ready in RX mode");
    RenderStatus("Board ready");
  } else {
    Serial.printf("LoRa radio init failed: %d\n", g_radioState);
    RenderStatus("Radio failed");
  }
}

void loop() {
  pulse::platform::PollBoard();

  if (pulse::platform::ConsumeButtonSingleClick()) {
    pulse::platform::BlinkConfirmationLed();
    RenderStatus("Button OK");
    g_lastDisplayRefreshMs = millis();
  }

  if (pulse::platform::ConsumeButtonLongPress(2000)) {
    g_radioReady = ConfigureDefaultRadio();
    RenderStatus(g_radioReady ? "Radio restarted" : "Radio failed");
    g_lastDisplayRefreshMs = millis();
  }

  const unsigned long now = millis();
  if (now - g_lastDisplayRefreshMs >= kDisplayRefreshIntervalMs) {
    g_lastDisplayRefreshMs = now;
    RenderStatus(g_radioReady ? "Board ready" : "Radio failed");
  }

  delay(10);
}
