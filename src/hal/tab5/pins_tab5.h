// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// M5Stack Tab5 and the Tab5 Keyboard. Sources: M5GFX (board_detect esp32p4.inl,
// Panel_ST7123/ST7121/ILI9881C, Touch_ST7123), M5Unified (M5Unified.inl,
// Power_Class.inl) and M5Tab5-Keyboard-Internal-FW. Spec:
// docs/superpowers/specs/2026-11-17-cyberspace-tab5-design.md.
namespace tab5 {

// System I2C: expanders, INA226, RTC, codec, touch.
constexpr int I2C_SDA = 31, I2C_SCL = 32;
constexpr uint32_t I2C_HZ = 100000;  // as the factory firmware (CONFIG_BSP_I2C_CLK_SPEED_HZ)

// PI4IOE5V6408 expanders.
constexpr uint8_t EXP1_ADDR = 0x43, EXP2_ADDR = 0x44;
constexpr uint8_t EXP1_ANTENNA = 0, EXP1_SPK_AMP = 1, EXP1_EXT_5V = 2, EXP1_LCD_RST = 4,
                  EXP1_TP_RST = 5, EXP1_CAM_RST = 6, EXP1_HP_DET = 7;
constexpr uint8_t EXP2_C6_POWER = 0, EXP2_USB_5V = 3, EXP2_POWER_OFF = 4, EXP2_CHG_STEP = 5,
                  EXP2_CHG_STATUS = 6, EXP2_CHG_EN = 7;

// MIPI-DSI panel, 720x1280 portrait. Which of three it is is found at boot.
constexpr int PANEL_W = 720, PANEL_H = 1280;
// The UI's logical panel, drawn 2x (ui/scale2x).
constexpr int NATIVE_W = PANEL_W / 2, NATIVE_H = PANEL_H / 2;
constexpr int DSI_LANES = 2;
constexpr int DSI_LDO_CHAN = 3, DSI_LDO_MV = 2500;
constexpr int BACKLIGHT = 22;
constexpr uint32_t BACKLIGHT_HZ = 5000;
constexpr uint8_t BACKLIGHT_BITS = 12;

// Touch: the ST7123/ST7121's own at 0x55, or a GT911 beside the ILI9881C.
constexpr uint8_t ST_TOUCH_ADDR = 0x55, GT911_ADDR = 0x14;
constexpr int TOUCH_INT = 23;
constexpr int TOUCH_RAW_W = PANEL_W, TOUCH_RAW_H = PANEL_H;

constexpr uint8_t INA226_ADDR = 0x41;
constexpr uint8_t RTC_ADDR = 0x32;  // RX8130CE
constexpr uint8_t CODEC_ADDR = 0x10;  // ES8388

// ES8388 speaker, NS4150B amplifier on EXP1_SPK_AMP.
constexpr int I2S_MCLK = 30, I2S_BCK = 27, I2S_WS = 29, I2S_DOUT = 26;

// ESP32-C6 over SDIO, powered by EXP2_C6_POWER.
constexpr int SDIO_CLK = 12, SDIO_CMD = 13, SDIO_D0 = 11, SDIO_D1 = 10, SDIO_D2 = 9,
              SDIO_D3 = 8, SDIO_RST = 15;

// microSD, SDMMC slot 0, 4-bit; pads powered by on-chip LDO 4. Slot 0's pins
// are fixed IO_MUX ones, listed for reference only.
constexpr int SD_CLK = 43, SD_CMD = 44, SD_D0 = 39, SD_D1 = 40, SD_D2 = 41, SD_D3 = 42;
constexpr int SD_LDO_CHAN = 4;

// Tab5 Keyboard on Ext.Port1 (Wire1), INT active low.
constexpr int KB_SDA = 0, KB_SCL = 1, KB_INT = 50;
constexpr uint32_t KB_HZ = 400000;
constexpr uint8_t KB_ADDR = 0x6D;

}  // namespace tab5

#endif
