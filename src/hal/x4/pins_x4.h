#pragma once

// Xteink X4 pins, from rhino-moshi's include/boards/x4.h (cross-checked there
// against CrossPoint's HalGPIO.h and Adafruit's CircuitPython library).
namespace x4 {

// GDEQ0426T82 e-paper, 800x480 native, used portrait (rotation 3): 480x800.
constexpr int EPD_SCLK = 8, EPD_MOSI = 10, EPD_CS = 21, EPD_DC = 4, EPD_RST = 5, EPD_BUSY = 6;
constexpr int SPI_MISO = 7;  // shared with the SD card
constexpr int SD_CS = 12;
constexpr int NATIVE_W = 800, NATIVE_H = 480;
constexpr int PANEL_W = NATIVE_H, PANEL_H = NATIVE_W;
constexpr unsigned EPD_SPI_HZ = 20000000;  // the SSD1677 takes writes up to 20 MHz

// Buttons: two resistor ladders on ADC pins (decoded by rc::x4ButtonMask),
// power on its own pin, active low.
constexpr int BUTTON_ADC_1 = 1;  // Back, Confirm, Left, Right
constexpr int BUTTON_ADC_2 = 2;  // Up, Down
constexpr int BUTTON_POWER = 3;

// Power: battery through a 2:1 divider; USB detect high on USB; the latch
// keeps the battery connected while HIGH and cuts it when LOW.
constexpr int BATTERY_ADC = 0;
constexpr float BATTERY_DIVIDER = 2.0f;
constexpr int USB_DETECT = 20;
constexpr int POWER_LATCH = 13;

}  // namespace x4
