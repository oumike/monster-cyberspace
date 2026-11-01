#pragma once

#include <cstdint>

// Attaky Cyber Deck Plus 1.0 pins. Source: Attaky datasheet
// "cyberdeck plus 1.0" (Core: ESP32 1.0, Expand: Media I/O 1.0,
// Control: Keyboard (Bottom) 1.0, Control: Gamepad (Bottom) 1.0,
// Power: Standard Cell 1.0).
//
// The build carries no GPS and no LoRa module, so IO17/IO18/IO21 are free for
// the Media I/O amplifier. On the Mesh Deck those same three pins are the GPS
// UART and the SX1262 DIO1, which is why a Mesh Deck build must never drive an
// amplifier there.
namespace cyber_deck_plus {

// ST7789, 240x320 portrait, alone on its own SPI pins (write-only).
constexpr int TFT_SCK = 11, TFT_MOSI = 13, TFT_CS = 45, TFT_DC = 12, TFT_RST = 10;
constexpr int TFT_BL = 14;  // active HIGH, PWM
constexpr uint32_t TFT_SPI_HZ = 40000000, TFT_BL_FREQ = 12000;
// The panel is mounted a half turn from the usual landscape: MADCTL MY|MV.
constexpr uint8_t PANEL_MADCTL = 0xA0;  // confirm on hardware (TESTING.md)
constexpr int PANEL_NATIVE_W = 240, PANEL_NATIVE_H = 320, PANEL_W = 320, PANEL_H = 240;
// FT6636 raw (portrait) to landscape: frame x = 319 - y, y = x.
constexpr uint8_t TOUCH_ROT = 0;  // confirm on hardware (TESTING.md)

// Main I2C: touch, the expanders, the fuel gauge, both Control modules.
constexpr int I2C_SDA = 2, I2C_SCL = 1;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint8_t TOUCH_ADDR = 0x38;     // FT6636, polled
constexpr uint8_t EXPANDER_ADDR = 0x59;  // AW9523: buttons P00-P07, LED P10-P12, touch reset P13
constexpr uint8_t GAUGE_ADDR = 0x36;     // MAX17048, Power: Standard Cell
constexpr uint8_t KB_LEFT_ADDR = 0x5A, KB_RIGHT_ADDR = 0x5B;  // keyboard module halves
constexpr uint8_t PAD_LEFT_ADDR = 0x22, PAD_RIGHT_ADDR = 0x26;  // gamepad halves (TCA9555)

// Expander port 0 is the front buttons, active low (rc::kPad* bits). P07 is
// the power button: never read as a key, its long hold cuts power.
constexpr uint8_t BTN_POWER_BIT = 1 << 7;
constexpr int BOOT_BTN = 38;  // a real GPIO, active low

// ── Expand: Media I/O 1.0 ───────────────────────────────────────────────
// NS4168 mono Class-D amplifier: BTB 31 / 32 / 39 -> IO17 / IO18 / IO21.
constexpr int SPK_BCLK = 17, SPK_SDATA = 18, SPK_LRCLK = 21;
constexpr uint32_t SPK_HZ = 16000;
// ICS-43434 microphone, on its own clocks: BTB 38/30/42/37 ->
// IO9 / IO16 / IO39 / IO46. Not driven yet (the app only outputs audio).
constexpr int MIC_WS = 9, MIC_SD = 16, MIC_SCK = 39, MIC_LR = 46;
// microSD slot: BTB 26/27/28/33 -> IO5 / IO6 / IO7 / IO8. Not mounted yet: the
// S3 has one general-purpose SPI peripheral and the panel already owns it.
constexpr int SD_CLK = 5, SD_CMD = 6, SD_DAT0 = 7, SD_DAT3 = 8;

}  // namespace cyber_deck_plus
