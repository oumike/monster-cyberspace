#pragma once

#include <cstdint>

// Seeed Wio Tracker L2 pins. Source: camillia-mt src/hal/hw_wio_tracker_l2.h
// (transcribed from Seeed's variant) and platy-aprs src/hal/pins_wio_l2.h.
//
// A PCA9555 expander gates almost every rail, so bring-up order is
// I2C -> expander -> everything else (io_pca9555.cpp).
namespace wio {

// Shared I2C: expander, touch, backlight driver, battery ADC, audio codec.
constexpr int I2C_SDA = 47, I2C_SCL = 48;
constexpr uint32_t I2C_HZ = 100000;

// PCA9555 expander and its bits.
constexpr uint8_t EXPANDER_ADDR = 0x21;
constexpr int EXPANDER_INT = 45;  // active LOW: an input changed
constexpr uint8_t EXP_WAKE_BUTTON = 0;  // in, active LOW
constexpr uint8_t EXP_I2C_INT = 1;      // in
constexpr uint8_t EXP_SD_DETECT = 2;    // in
constexpr uint8_t EXP_TOUCH_INT = 3;    // driven LOW; touch is polled
constexpr uint8_t EXP_LCD_CS = 4;
constexpr uint8_t EXP_LCD_POWER = 5;
constexpr uint8_t EXP_LCD_RST = 6;
constexpr uint8_t EXP_GROVE_POWER = 7;
constexpr uint8_t EXP_TOUCH_RST = 8;
constexpr uint8_t EXP_GNSS_RST = 9;  // active HIGH
constexpr uint8_t EXP_USER_LED = 10;
constexpr uint8_t EXP_USB_OTG_EN = 11;
constexpr uint8_t EXP_AUDIO_PA = 12;
constexpr uint8_t EXP_GNSS_POWER = 13;
constexpr uint8_t EXP_SD_POWER = 14;
constexpr uint8_t EXP_BATT_SENSE_EN = 15;

// 1-bit SD_MMC: dedicated pins, no chip select. Power is EXP_SD_POWER.
constexpr int SDMMC_CLK = 2, SDMMC_CMD = 3, SDMMC_D0 = 1;

// GPS on Serial1; power and reset are expander bits.
constexpr int GPS_RX = 18, GPS_TX = 17;
constexpr uint32_t GPS_BAUD = 9600;

// NV3031B, 240x320 native, on quad SPI: no DC line, reset on the expander.
constexpr int TFT_SCK = 42;
constexpr int TFT_IO0 = 41, TFT_IO1 = 40, TFT_IO2 = 39, TFT_IO3 = 38;
constexpr int TFT_CS = 46;
constexpr uint32_t TFT_WRITE_HZ = 75000000;
constexpr int PANEL_NATIVE_W = 240, PANEL_NATIVE_H = 320;
// Driven landscape, like the M9.
constexpr int PANEL_W = 320, PANEL_H = 240;

// LP5814 I2C backlight driver (four channels).
constexpr uint8_t LP5814_ADDR = 0x2C;
constexpr uint8_t BACKLIGHT_ON = 160;  // camillia-mt's default brightness

// GT911 touch. Its address depends on INT during reset: held LOW -> 0x5D.
constexpr uint8_t TOUCH_ADDR = 0x5D, TOUCH_ADDR_ALT = 0x14;

// ADS1115 battery ADC: channel 0 sees battery / 2; sense gated by the expander.
constexpr uint8_t ADS1115_ADDR = 0x48;
constexpr float BATT_DIV = 2.0f;

// ES8311 codec + amplifier on I2S. The amp's power (EXP_AUDIO_PA) needs 250 ms
// to settle. The ES7243E microphone is not used.
constexpr uint8_t CODEC_ADDR = 0x18;
constexpr int I2S_MCLK = 10, I2S_BCK = 11, I2S_WS = 12, I2S_DOUT = 16;
constexpr uint32_t AMP_SETTLE_MS = 250;

// The only MCU-side button.
constexpr int BOOT_BUTTON = 0;

}  // namespace wio
