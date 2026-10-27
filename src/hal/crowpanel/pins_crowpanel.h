#pragma once

#include <cstdint>

// Elecrow CrowPanel Advance 3.5" and 5.0" HMI (ESP32-S3-WROOM-1-N16R8).
// Source: camillia-mt src/hal/hw_crowpanel_35.h, hw_crowpanel_50.h and
// crowpanel_50_display.h (from Meshtastic's elecrow_panel variant and
// Elecrow's V1.0 schematics and factory code). No keyboard, buttons, battery
// sense or RTC; the LoRa slot is unused here.
namespace crowpanel {

// Both sizes: GT911 touch on I2C, polled.
constexpr int I2C_SDA = 15, I2C_SCL = 16;
constexpr uint8_t TOUCH_ADDR = 0x5D;

namespace p35 {
// ILI9488, 320x480 native portrait, on SPI3 (write-only).
constexpr int TFT_SCK = 42, TFT_MOSI = 39, TFT_CS = 40, TFT_DC = 41;
constexpr uint32_t TFT_WRITE_HZ = 40000000;  // Meshtastic runs 60 MHz
constexpr int TFT_BL = 38;
constexpr uint32_t TFT_BL_FREQ = 44000;
constexpr uint8_t BACKLIGHT_ON = 160;
constexpr int PANEL_NATIVE_W = 320, PANEL_NATIVE_H = 480;
constexpr int PANEL_W = 480, PANEL_H = 320;  // landscape
// GT911 strap pins: INT held LOW through reset gives 0x5D.
constexpr int TOUCH_INT = 47, TOUCH_RST = 48;
// microSD (J5) on the free SPI host.
constexpr int SD_SCK = 5, SD_MISO = 4, SD_MOSI = 6, SD_CS = 7;
constexpr uint32_t SD_SPI_HZ = 20000000;
// Optional GPS module on the UART1 connector.
constexpr int GPS_RX = 18, GPS_TX = 17;
constexpr uint32_t GPS_BAUD = 9600;
}  // namespace p35

namespace p50 {
// 800x480 RGB565 parallel panel (ST7262), native landscape, scanned out of a
// PSRAM framebuffer by the S3's LCD peripheral.
constexpr int PANEL_W = 800, PANEL_H = 480;
constexpr uint32_t PCLK_HZ = 14000000;  // Elecrow 21 MHz, Meshtastic 14 MHz
constexpr int PIN_DE = 42, PIN_VSYNC = 41, PIN_HSYNC = 40, PIN_PCLK = 39;
// D0..D15: B0-B4, G0-G5, R0-R4.
constexpr int PIN_DATA[16] = {21, 47, 48, 45, 38, 9, 10, 11, 12, 13, 14, 7, 17, 18, 3, 46};
// GT911 INT doubles as its address strap; reset is on the board controller.
constexpr int TOUCH_INT = 1;
// The backlight and touch reset belong to a PCA9557 (V1.0) or a board MCU
// (V1.1 and later), told apart by which address answers.
constexpr uint8_t IOEX_ADDR = 0x18;
constexpr uint8_t IOEX_BL = 1, IOEX_TP_RST = 2, IOEX_AMP_MUTE = 3, IOEX_AMP_SHUT = 4;
constexpr uint8_t MCU_ADDR = 0x30;
// MCU commands (V1.2/V1.3): 0 brightest .. 244 dimmest, 245 off, 250 touch on.
constexpr uint8_t MCU_BL_MAX = 0, MCU_BL_OFF = 245, MCU_TOUCH_ON = 250;
}  // namespace p50

}  // namespace crowpanel
