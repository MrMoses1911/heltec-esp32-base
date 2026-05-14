#include "Arduino.h"
#include "platform/platform_bootstrap.h"

void setup() {
  Serial.begin(115200);
  pulse::platform::InitializeBoard();
  Serial.println("Heltec WiFi LoRa V3 — ready");
}

void loop() {
  pulse::platform::PollBoard();
  delay(10);
}
