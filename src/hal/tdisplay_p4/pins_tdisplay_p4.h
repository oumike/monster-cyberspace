#pragma once

#include <cstdint>

// LilyGO T-Display P4 V1.0 (ECO2), 4.1" AMOLED. From camillia-mt's
// src/hal/hw_tdisplay_p4.h (itself from LilyGO's t_display_p4_config.h).
namespace tdisplay_p4 {

// Main I2C bus: the XL9535 expander, GT9895 touch, BQ27220 gauge.
constexpr int I2C_SDA = 7, I2C_SCL = 8;
constexpr uint32_t I2C_HZ = 400000;

// XL9535 (XL9555 registers) bits.
constexpr uint8_t EXPANDER_ADDR = 0x20;
constexpr uint8_t EXP_POWER_3V3 = 0, EXP_ANTENNA_SELECT = 1, EXP_SCREEN_RST = 2, EXP_TOUCH_RST = 3,
                  EXP_TOUCH_INT = 4, EXP_ETHERNET_RST = 5, EXP_AUDIO_POWER = 6, EXP_IMU_INT = 7,
                  EXP_USB_PHY_POWER = 8, EXP_GPS_WAKE = 9, EXP_RTC_INT = 10, EXP_C6_WAKE = 11,
                  EXP_C6_EN = 12, EXP_SD_POWER = 13 /* active low */, EXP_RADIO_RST = 14,
                  EXP_RADIO_DIO1 = 15;

// RM69A10 AMOLED over two-lane MIPI-DSI, native portrait.
constexpr int PANEL_W = 568, PANEL_H = 1232;
// The UI draws at half resolution (284x616 portrait), each pixel doubled:
// at ~330 ppi the cell fonts are unreadable at 1:1.
constexpr int SCALE = 2;
constexpr int DSI_LANES = 2, DSI_LANE_MBPS = 1000, DSI_DPI_MHZ = 80;
constexpr int DSI_HSYNC = 50, DSI_HBP = 150, DSI_HFP = 50, DSI_VSYNC = 40, DSI_VBP = 120, DSI_VFP = 80;

// GT9895 touch, raw 1060x2400 over the native portrait panel.
constexpr uint8_t TOUCH_ADDR = 0x5D;
constexpr int TOUCH_RAW_W = 1060, TOUCH_RAW_H = 2400;

constexpr uint8_t GAUGE_ADDR = 0x55;  // BQ27220

// L76K GNSS on UART1: the P4 listens on 22, sends on 23.
constexpr int GPS_RX = 22, GPS_TX = 23;
constexpr uint32_t GPS_BAUD = 9600;

// Four-bit SDMMC host 0; power through EXP_SD_POWER.
constexpr int SD_CLK = 43, SD_CMD = 44, SD_D0 = 39, SD_D1 = 40, SD_D2 = 41, SD_D3 = 42;

// ES8311 codec on its own I2C pins (Wire1); amplifier power EXP_AUDIO_POWER.
constexpr int AUDIO_SDA = 20, AUDIO_SCL = 21;
constexpr uint8_t CODEC_ADDR = 0x18;
constexpr int I2S_MCLK = 13, I2S_BCK = 12, I2S_WS = 9, I2S_DOUT = 10;
constexpr uint32_t AMP_SETTLE_MS = 8;

constexpr int USER_BUTTON = 35;  // active low

}  // namespace tdisplay_p4
