#pragma once

#include <cstdint>

// LilyGO T-LoRa Pager pins. Sources: camillia-mt src/hal/hw_tlora_pager.h and
// arduino-esp32's variants/lilygo_tlora_pager/pins_arduino.h.
namespace pager {

// ST7796 on the SPI bus it shares with LoRa and SD. The 222x480 glass sits
// in the middle of the controller's 320x480 memory (49 columns each side).
constexpr int SPI_SCK = 35, SPI_MOSI = 34, SPI_MISO = 33;
constexpr uint32_t TFT_SPI_HZ = 40000000;
constexpr int TFT_CS = 38, TFT_DC = 37, TFT_RST = -1;
constexpr int TFT_BL = 42;
constexpr uint32_t TFT_BL_FREQ = 12000;
constexpr int LORA_CS = 36, SD_CS = 21, NFC_CS = 39;
constexpr int PANEL_W = 480, PANEL_H = 222;
constexpr uint16_t PANEL_ROW_OFFSET = 49;
// MADCTL MY|MV|ML|BGR: LovyanGFX's rotation 3, camillia's landscape for this
// board (TFT_ROTATION_DEFAULT 3, rgb_order false).
constexpr uint8_t PANEL_MADCTL = 0xB8;  // confirm on hardware (TESTING.md)

// One I2C bus: keyboard, expander, charger, RTC and codec.
constexpr int I2C_SDA = 3, I2C_SCL = 2;
constexpr uint32_t I2C_HZ = 400000;

// XL9555 expander, address found in 0x20-0x27; bit numbers.
constexpr uint8_t EXP_DRV_EN = 0, EXP_AMP_EN = 1, EXP_KB_RST = 2, EXP_LORA_EN = 3,
                  EXP_GPS_EN = 4, EXP_NFC_EN = 5, EXP_GPS_RST = 7, EXP_KB_EN = 8,
                  EXP_GPIO_EN = 9, EXP_SD_DET = 10, EXP_SD_PULLEN = 11, EXP_SD_EN = 12;

// TCA8418 keyboard; its interrupt line is low while events wait.
constexpr uint8_t KB_ADDR = 0x34;
constexpr int KB_INT = 6, KB_BL = 46;  // backlight HIGH = on

// Side wheel: quadrature A/B and its press, all active low; BOOT, active low.
constexpr int WHEEL_A = 40, WHEEL_B = 41, WHEEL_CLICK = 7, BOOT_BUTTON = 0;

// GPS on Serial1, powered through EXP_GPS_EN.
constexpr int GPS_RX = 4, GPS_TX = 12;
constexpr uint32_t GPS_BAUD = 38400;

// BQ25896 charger (battery voltage from its ADC) and PCF85063A RTC.
constexpr uint8_t CHARGER_ADDR = 0x6B, RTC_ADDR = 0x51;

// ES8311 codec on I2S; the speaker amp is EXP_AMP_EN.
constexpr uint8_t CODEC_ADDR = 0x18;
constexpr int I2S_MCLK = 10, I2S_BCK = 11, I2S_WS = 18, I2S_DOUT = 45;
constexpr uint32_t AMP_SETTLE_MS = 8;

}  // namespace pager
