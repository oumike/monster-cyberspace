#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// LilyGO T-Display P4 V1.0 (AMOLED). Sources: wadamesh variants/tdisplay_p4/
// (tested on hardware, authoritative) and camillia-mt src/hal/hw_tdisplay_p4.h.
// LilyGO numbers the XL9535 pins in octal (IO00-IO07, IO10-IO17); the EXP_
// constants here are bit numbers 0-15.
namespace tdp4 {

// I2C0: XL9535, GT9895 touch, BQ27220 gauge, PCF8563 RTC.
constexpr int I2C_SDA = 7, I2C_SCL = 8;
constexpr uint32_t I2C_HZ = 400000;

// XL9535 expander.
constexpr uint8_t EXP_ADDR = 0x20;
constexpr int EXP_INT = 5;
constexpr uint8_t EXP_3V3 = 0, EXP_ANTENNA = 1, EXP_SCREEN_RST = 2, EXP_TOUCH_RST = 3,
                  EXP_TOUCH_INT = 4, EXP_ETHERNET_RST = 5, EXP_AUDIO_POWER = 6, EXP_IMU_INT = 7,
                  EXP_VCCA = 8, EXP_GPS_WAKE = 9, EXP_RTC_INT = 10, EXP_C6_WAKE = 11,
                  EXP_C6_EN = 12, EXP_SD_POWER = 13, EXP_RADIO_RST = 14, EXP_RADIO_DIO1 = 15;
// EXP_AUDIO_POWER is the rail wadamesh calls 5V_EN (LilyGO IO06).

// RM69A10 AMOLED, MIPI-DSI.
constexpr int PANEL_W = 568, PANEL_H = 1232;
// The UI's logical panel, drawn 2x (ui/scale2x).
constexpr int NATIVE_W = PANEL_W / 2, NATIVE_H = PANEL_H / 2;
constexpr int DSI_LANES = 2;
constexpr uint32_t DSI_LANE_MBPS = 1000;
constexpr uint32_t DPI_MHZ = 60;
constexpr int HSYNC = 50, HBP = 150, HFP = 50;
constexpr int VSYNC = 40, VBP = 120, VFP = 80;
// DSI-PHY LDO: 2500 mV leaves the panel dark (wadamesh).
constexpr int DSI_LDO_CHAN = 3, DSI_LDO_MV = 1830;

// GT9895 touch on I2C0.
constexpr uint8_t TOUCH_ADDR = 0x5D;
constexpr int TOUCH_RAW_W = 1060, TOUCH_RAW_H = 2400;

constexpr uint8_t GAUGE_ADDR = 0x55;  // BQ27220
constexpr uint8_t RTC_ADDR = 0x51;    // PCF8563

// Wire1, shared in turn: the codec and haptic driver on 20/21, the keyboard
// expansion on 46/45.
constexpr int AUDIO_SDA = 20, AUDIO_SCL = 21;
constexpr uint8_t CODEC_ADDR = 0x18, HAPTIC_ADDR = 0x58;
constexpr int I2S_MCLK = 13, I2S_BCK = 12, I2S_WS = 9, I2S_DOUT = 10;
constexpr int KB_SDA = 46, KB_SCL = 45, KB_INT = 48, KB_BL = 47;
constexpr uint8_t KB_ADDR = 0x34, KB_EXP_ADDR = 0x20;
constexpr uint8_t KB_EXP_TCA_RST = 6;

// L76K GPS: the P4 listens on 22 and sends on 23.
constexpr int GPS_RX = 22, GPS_TX = 23;
constexpr uint32_t GPS_BAUD = 9600;

// microSD, SDMMC slot 0, 4-bit. These are the slot's IO_MUX pins, fixed in
// silicon; its pads (GPIO39-48) run from the on-chip LDO channel 4.
constexpr int SD_CLK = 43, SD_CMD = 44, SD_D0 = 39, SD_D1 = 40, SD_D2 = 41, SD_D3 = 42;
constexpr int SD_LDO_CHAN = 4;

// SX1262, unused: reset (EXP_RADIO_RST), then put to sleep.
constexpr int LORA_SCK = 2, LORA_MOSI = 3, LORA_MISO = 4, LORA_CS = 24, LORA_BUSY = 6;

// Side button, active low.
constexpr int SIDE_BUTTON = 35;

}  // namespace tdp4

#endif
