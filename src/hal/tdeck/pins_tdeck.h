#pragma once

#include <cstdint>

// LilyGO T-Deck Plus pins. Source: camillia-mt src/hal/hw_tdeck.h.
namespace tdeck {

constexpr int POWERON = 10;  // peripheral rail (GPS included), HIGH first

// ST7789 on the shared SPI bus with LoRa and SD.
constexpr int SPI_SCK = 40, SPI_MOSI = 41, SPI_MISO = 38;
constexpr uint32_t TFT_SPI_HZ = 40000000;
constexpr int TFT_CS = 12, TFT_DC = 11, TFT_RST = -1;
constexpr int TFT_BL = 42;
constexpr uint32_t TFT_BL_FREQ = 12000;
constexpr int LORA_CS = 9, SD_CS = 39;
constexpr uint32_t SD_SPI_HZ = 20000000;

// MADCTL MX|MV: LovyanGFX rotation 1, camillia's landscape for this board.
constexpr uint8_t PANEL_MADCTL = 0x60;  // confirm on hardware (TESTING.md)
constexpr int PANEL_NATIVE_W = 240, PANEL_NATIVE_H = 320, PANEL_W = 320, PANEL_H = 240;
// GT911 raw → landscape: touchToFrame(raw, NATIVE_W, NATIVE_H, TOUCH_ROT). The
// Wio's GT911 sits a half turn from this one (LovyanGFX touch offset 2 there,
// 0 here, at the same panel rotation), hence 2.
constexpr uint8_t TOUCH_ROT = 2;  // confirm on hardware (TESTING.md)

// Keyboard (ESP32-C3) and GT911 on Wire.
constexpr int I2C_SDA = 18, I2C_SCL = 8;
constexpr uint8_t KB_ADDR = 0x55;
constexpr int KB_INT = 46;  // unused: polled
constexpr uint8_t TOUCH_ADDR = 0x5D;
constexpr int TOUCH_INT = 16;  // unused: polled

// Trackball, active low: right → 2, left → 1, up → 3, down → 15. Up and down
// are the reverse of camillia's map, confirmed on hardware (2026-10-27).
constexpr int TB_RIGHT = 2, TB_LEFT = 1, TB_UP = 3, TB_DOWN = 15, TB_CLICK = 0;

// L76K GPS on Serial1.
constexpr int GPS_RX = 44, GPS_TX = 43;
constexpr uint32_t GPS_BAUD = 38400;

// Battery: 1:2 divider, scaled up for the divider's slow settling.
constexpr int BATT_ADC = 4;
constexpr float BATT_DIV = 2.0f, BATT_CAL = 1.03f;

// I2S speaker amplifier.
constexpr int I2S_WS = 5, I2S_BCK = 7, I2S_DOUT = 6;

}  // namespace tdeck
