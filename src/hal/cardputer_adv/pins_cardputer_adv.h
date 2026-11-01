#pragma once

#include <cstdint>

// M5Stack Cardputer ADV pins (ESP32-S3FN8, no PSRAM). Spec
// docs/superpowers/specs/2026-11-01-cyberspace-cardputer-adv-design.md §2.
namespace cardputer {

// ST7789V2, 240x135 visible, on its own SPI bus. In portrait the glass starts
// at column 53 (the spec's "X 53"), row 40; turned landscape (MV) those swap,
// so the window starts at column 40, row 53 (TFT_eSPI's 135x240 rotation 1).
constexpr int TFT_SCK = 36, TFT_MOSI = 35, TFT_CS = 37, TFT_DC = 34, TFT_RST = 33;
constexpr uint32_t TFT_SPI_HZ = 40000000;
// The backlight PWM, which also enables the BL_3V3 rail.
constexpr int TFT_BL = 38;
constexpr uint32_t TFT_BL_FREQ = 12000;
constexpr int PANEL_W = 240, PANEL_H = 135;
constexpr uint16_t PANEL_COL_OFFSET = 40, PANEL_ROW_OFFSET = 53;
// MADCTL MX|MV (0x60), RGB: landscape with the keyboard below the screen, as
// TFT_eSPI's rotation 1. Confirm on hardware (TESTING.md): 0xA0 (MY|MV, row
// offset 52) is the other landscape, and 0x08 swaps red and blue.
constexpr uint8_t PANEL_MADCTL = 0x60;

// One I2C bus: keyboard and codec (the BMI270 on it is unused).
constexpr int I2C_SDA = 8, I2C_SCL = 9;
constexpr uint32_t I2C_HZ = 400000;

// TCA8418 keyboard; its interrupt line is low while events wait.
constexpr uint8_t KB_ADDR = 0x34;
constexpr int KB_INT = 11;

// ES8311 codec on I2S, with no MCLK line: it clocks itself from BCLK.
constexpr uint8_t CODEC_ADDR = 0x18;
constexpr int I2S_BCK = 41, I2S_WS = 43, I2S_DOUT = 42, I2S_DIN = 46;

// Battery through a 1:2 divider on ADC1.
constexpr int BATT_ADC = 10;
constexpr float BATT_DIV = 2.0f;

// microSD, on the bus the expansion port shares: an M5Stack LoRa-1262 Cap's
// SX1262 sits on it with its chip select on GPIO5, held high so a fitted cap
// can't drive MISO (camillia-mt's hw_cardputer.h, config_io.cpp sdBegin).
constexpr int SD_SCK = 40, SD_MOSI = 14, SD_MISO = 39, SD_CS = 12;
constexpr int CAP_LORA_CS = 5;

}  // namespace cardputer
