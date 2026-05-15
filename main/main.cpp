#include "Arduino.h"
#include "platform/platform_bootstrap.h"
#include "sensors/q5x_pfm.h"

Q5X_PFM g_sensor;

void setup() {
  Serial.begin(115200);
  pulse::platform::InitializeBoard();
  g_sensor.init();
}

void loop() {
  pulse::platform::PollBoard();
  g_sensor.update();

  static uint32_t lastDisplayMs = 0;
  uint32_t now = millis();
  if (now - lastDisplayMs >= 250) {
    lastDisplayMs = now;
    float dist = g_sensor.getDistanceMm();
    auto status = g_sensor.getStatus();

    pulse::platform::DisplayClear();
    pulse::platform::DisplaySetFontNormal();
    pulse::platform::DisplayDrawText(0, 0, "Distancia");

    if (status == Q5X_PFM::kOk) {
      pulse::platform::DisplaySetFontLarge();
      char buf[16];
      snprintf(buf, sizeof(buf), "%.0f mm", static_cast<double>(dist));
      pulse::platform::DisplayDrawText(4, 18, buf);
    } else if (status == Q5X_PFM::kNoSignal) {
      pulse::platform::DisplayDrawText(0, 24, "NO SIGNAL");
    } else if (status == Q5X_PFM::kInvalid) {
      pulse::platform::DisplayDrawText(0, 24, "INVALID");
    } else {
      pulse::platform::DisplayDrawText(0, 24, "NO INIT");
    }

    float freq = g_sensor.getFrequencyHz();
    if (freq > 0.0f) {
      pulse::platform::DisplaySetFontNormal();
      char buf2[16];
      snprintf(buf2, sizeof(buf2), "%.1f Hz", static_cast<double>(freq));
      pulse::platform::DisplayDrawText(0, 54, buf2);
    }

    pulse::platform::DisplayPresent();
  }

  delay(10);
}
