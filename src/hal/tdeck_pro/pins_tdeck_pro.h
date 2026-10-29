#pragma once

#include <cstdint>

// LilyGO T-Deck Pro: ESP32-S3, 16 MB flash, 8 MB quad-SPI PSRAM, a 3.1"
// GDEQ031T10 240x320 monochrome e-paper (portrait) with a PWM frontlight,
// CST328 or CST3530 touch, TCA8418 keyboard, u-blox MIA-M10Q GPS, BQ25896
// charger and BQ27220 gauge, microSD, SX1262 (unused). No speaker, no RTC.
// From camillia-mt's hw_tdeck_pro.h and platy-aprs's pins_tdeck_pro.h.
namespace tdeck_pro {

constexpr int PANEL_W = 240, PANEL_H = 320;

// Shared SPI bus: e-paper, SX1262 and microSD.
constexpr int SPI_SCK = 36, SPI_MISO = 47, SPI_MOSI = 33;
constexpr int EPD_CS = 34, EPD_DC = 35, EPD_RST = 16, EPD_BUSY = 37;
constexpr uint32_t EPD_SPI_HZ = 2000000;
constexpr int FRONTLIGHT = 45;  // active high; kept off
constexpr int SD_CS = 48;
constexpr int LORA_CS = 3;
constexpr int LORA_POWER = 46;  // the radio module's supply, active high

// Keyboard, touch, charger and gauge share one I2C bus.
constexpr int I2C_SDA = 13, I2C_SCL = 14;
constexpr uint32_t I2C_HZ = 400000;
constexpr uint8_t KB_ADDR = 0x34;
constexpr int KB_INT = 15;  // active low
constexpr int KB_BL = 42;   // keyboard backlight; kept off
constexpr uint8_t TOUCH_ADDR = 0x1A;
constexpr int TOUCH_INT = 12, TOUCH_RST = 38;
constexpr uint8_t CHARGER_ADDR = 0x6B;  // BQ25896
constexpr uint8_t GAUGE_ADDR = 0x55;    // BQ27220

// The side button: locks and wakes the screen.
constexpr int SIDE_BUTTON = 0;  // active low

// MIA-M10Q on UART1, with a power enable.
constexpr int GPS_RX = 44, GPS_TX = 43, GPS_EN = 39;  // GPS_EN active high
constexpr uint32_t GPS_BAUD = 38400;

}  // namespace tdeck_pro
