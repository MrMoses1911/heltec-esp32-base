#ifndef TEMPLATE_PLATFORM_BOOTSTRAP_H
#define TEMPLATE_PLATFORM_BOOTSTRAP_H

#include <cstddef>
#include <cstdint>

namespace pulse {
namespace platform {

void InitializeBoard();
void PollBoard();
void BlinkConfirmationLed();
bool ConsumeButtonSingleClick();
bool ConsumeButtonLongPress(uint32_t durationMs);
bool IsButtonPressed();

void DisplayClear();
void DisplaySetFontNormal();
void DisplaySetFontLarge();
void DisplayDrawText(int16_t x, int16_t y, const char* text);
int16_t DisplayMeasureTextWidth(const char* text);
void DisplayDrawImage(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t* image);
void DisplayDrawPageImage(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t* image);
void DisplayPresent();

int InitializeRadio();
int ConfigureRadio(float frequencyMhz, float bandwidthKhz, uint8_t spreadingFactor, int8_t outputPowerDbm);
void SetRadioReceiveAction(void (*func)(void));
void ClearRadioReceiveAction();
int StartRadioReceive();
int TransmitRadio(const uint8_t* data, size_t length);
size_t GetRadioPacketLength();
int ReadRadioData(uint8_t* data, size_t length);
float GetRadioRssi();
float GetRadioSnr();

}  // namespace platform
}  // namespace pulse

#endif
