#pragma once

#include <cstdint>

// ThinkNode M9 pins. Source: camillia-mt src/hal/hw_m9.h (verified on hardware).
namespace m9 {

constexpr int VEXT = 18;  // LCD/GPS/sensor rail, active LOW
constexpr int BUZZER = 9;

constexpr int SPI_SCK = 40, SPI_MISO = 38, SPI_MOSI = 47;
constexpr uint32_t TFT_SPI_HZ = 40000000;
constexpr int TFT_CS = 16, TFT_DC = 15, TFT_RST = 14;
constexpr int TFT_BL = 17;  // PNP, active LOW
constexpr int TFT_BL_PWM_CH = 7;
constexpr int TFT_BL_FREQ = 12000;
constexpr int SD_CS = 48, LORA_CS = 39;

constexpr int KB_SDA = 20, KB_SCL = 21, KB_INT = 12;
constexpr uint8_t KB_ADDRS[] = {0x6C, 0x6D};
constexpr uint8_t KB_REG_HW = 0x00, KB_REG_FW = 0xFE;
constexpr uint8_t KB_REG_KEY = 0x05, KB_REG_KEY_EARLY = 0x01;

// GPS on Serial1. Power is active LOW; reset is active HIGH through an NPN.
constexpr int GPS_RX = 2, GPS_TX = 3;
constexpr uint32_t GPS_BAUD = 115200;
constexpr int GPS_EN = 11, GPS_RST = 5;

// PCF8563 RTC on Wire1 (Wire is the keyboard bus).
constexpr uint8_t RTC_ADDR = 0x51;
constexpr int RTC_SDA = 7, RTC_SCL = 6;
constexpr uint32_t RTC_I2C_HZ = 100000;

constexpr int BATT_ADC = 13;
constexpr float BATT_DIV = 2.0f;

constexpr int PANEL_W = 320, PANEL_H = 240;

}  // namespace m9
