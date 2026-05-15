#include "platform/platform_bootstrap.h"

#include "Arduino.h"
#include "heltec_unofficial.h"

namespace pulse {
namespace platform {

namespace {

constexpr unsigned long kBoardPowerSettleMs = 20UL;
constexpr int kConfirmationLedBrightnessPercent = 100;
constexpr unsigned long kConfirmationBlinkIntervalMs = 150UL;
constexpr uint8_t kConfirmationBlinkCount = 3;

}  // namespace

void InitializeBoard() {
  Serial.begin(115200);
#ifndef ARDUINO_heltec_wifi_32_lora_V3
  hspi->begin(SCK, MISO, MOSI, SS);
#endif
  heltec_ve(true);
  delay(kBoardPowerSettleMs);
  heltec_display_power(true);
  display.init();
  display.setContrast(255);
  display.flipScreenVertically();
  display.displayOn();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.display();
}

void PollBoard() {
  heltec_loop();
}

void BlinkConfirmationLed() {
  for (uint8_t i = 0; i < kConfirmationBlinkCount; ++i) {
    heltec_led(kConfirmationLedBrightnessPercent);
    delay(kConfirmationBlinkIntervalMs);
    heltec_led(0);
    delay(kConfirmationBlinkIntervalMs);
  }
}

bool ConsumeButtonSingleClick() {
  return button.isSingleClick();
}

bool ConsumeButtonLongPress(uint32_t durationMs) {
  return button.pressedFor(static_cast<int>(durationMs));
}

bool IsButtonPressed() {
  pinMode(BUTTON, INPUT_PULLUP);
  bool first = digitalRead(BUTTON) == LOW;
  delayMicroseconds(50);
  bool second = digitalRead(BUTTON) == LOW;
  return first && second;
}

void DisplaySetFontNormal() {
  display.setFont(ArialMT_Plain_10);
}

void DisplaySetFontLarge() {
  display.setFont(ArialMT_Plain_16);
}

void DisplayClear() {
  display.clear();
}

void DisplayDrawText(int16_t x, int16_t y, const char* text) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(x, y, text == nullptr ? "" : String(text));
}

int16_t DisplayMeasureTextWidth(const char* text) {
  return static_cast<int16_t>(display.getStringWidth(text == nullptr ? "" : String(text)));
}

void DisplayDrawImage(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t* image) {
  if (image == nullptr || width <= 0 || height <= 0) return;
  display.drawFastImage(x, y, width, height, image);
}

void DisplayPresent() {
  display.displayOn();
  display.display();
}

int InitializeRadio() {
  return radio.begin();
}

int ConfigureRadio(float frequencyMhz, float bandwidthKhz, uint8_t spreadingFactor, int8_t outputPowerDbm) {
  int state = radio.setFrequency(frequencyMhz);
  if (state != 0) return state;
  state = radio.setBandwidth(bandwidthKhz);
  if (state != 0) return state;
  state = radio.setSpreadingFactor(spreadingFactor);
  if (state != 0) return state;
  return radio.setOutputPower(outputPowerDbm);
}

void SetRadioReceiveAction(void (*func)(void)) {
  radio.setDio1Action(func);
}

void ClearRadioReceiveAction() {
  radio.clearDio1Action();
}

int StartRadioReceive() {
  return radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
}

int TransmitRadio(const uint8_t* data, size_t length) {
  return radio.transmit(data, length);
}

size_t GetRadioPacketLength() {
  return radio.getPacketLength();
}

int ReadRadioData(uint8_t* data, size_t length) {
  return radio.readData(data, length);
}

float GetRadioRssi() {
  return radio.getRSSI();
}

float GetRadioSnr() {
  return radio.getSNR();
}

}  // namespace platform
}  // namespace pulse
