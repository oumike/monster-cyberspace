#pragma once

#include <cstdint>

// Attaky Mesh Deck 1.0 pins. Sources: the Attaky hardware catalog,
// camillia-mt src/hal/hw_mesh_deck.h, platy-aprs src/hal/pins_mesh_deck.h.
namespace mesh_deck {

// ST7789, 240x320 portrait, alone on its own SPI pins (write-only).
constexpr int TFT_SCK = 11, TFT_MOSI = 13, TFT_CS = 45, TFT_DC = 12, TFT_RST = 10;
constexpr int TFT_BL = 14;  // active HIGH, PWM
constexpr uint32_t TFT_SPI_HZ = 40000000, TFT_BL_FREQ = 12000;
// The panel is mounted a half turn from the usual landscape: LovyanGFX
// rotation 3, MADCTL MY|MV.
constexpr uint8_t PANEL_MADCTL = 0xA0;  // confirm on hardware (TESTING.md)
constexpr int PANEL_NATIVE_W = 240, PANEL_NATIVE_H = 320, PANEL_W = 320, PANEL_H = 240;
// FT6636 raw (portrait) to landscape: frame x = 319 - y, y = x (platy-aprs).
constexpr uint8_t TOUCH_ROT = 0;  // confirm on hardware (TESTING.md)

// Main I2C: touch, the expanders, the fuel gauge.
constexpr int I2C_SDA = 2, I2C_SCL = 1;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint8_t TOUCH_ADDR = 0x38;     // FT6636, polled
constexpr uint8_t EXPANDER_ADDR = 0x59;  // AW9523: buttons P00-P07, LED P10-P12, touch reset P13
constexpr uint8_t GAUGE_ADDR = 0x36;     // MAX17048
constexpr uint8_t KB_LEFT_ADDR = 0x5A, KB_RIGHT_ADDR = 0x5B;  // keyboard module halves

// Expander port 0 is the front buttons, active low (rc::kPad* bits). P07 is
// the power button: never read as a key, its long hold cuts power.
constexpr uint8_t BTN_POWER_BIT = 1 << 7;
constexpr int BOOT_BTN = 38;  // a real GPIO, active low

// ATGM336H GPS, always powered.
constexpr int GPS_RX = 17, GPS_TX = 18;
constexpr uint32_t GPS_BAUD = 9600;

// The LoRa/GPS module's Ra-01SH (SX1262) on its own SPI pins (camillia-mt
// hw_mesh_deck.h). The firmware doesn't use the radio, and not every Mesh
// Deck has one: radioSleep() puts it to sleep at boot, if it answers.
constexpr int LORA_SCK = 5, LORA_MISO = 7, LORA_MOSI = 6, LORA_CS = 8;
constexpr int LORA_RST = 9, LORA_BUSY = 16, LORA_DIO1 = 21;

}  // namespace mesh_deck
