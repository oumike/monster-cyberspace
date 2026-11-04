#pragma once
#if defined(BOARD_TANMATSU)

#include <cstdint>

// Nicolai Electronics Tanmatsu (ESP32-P4). Sources: badge-bsp
// targets/tanmatsu/tanmatsu_hardware.h, the tanmatsu_coprocessor component and
// the mipi_dsi_abstraction ST7701 panel, as wadamesh's tanmatsu/ build pins them.
namespace tanmatsu {

// Internal I2C: the coprocessor and the ES8156 (and an unused BMI270).
constexpr int I2C_SDA = 9, I2C_SCL = 10;
constexpr uint32_t I2C_HZ = 400000;
constexpr uint8_t COPROC_ADDR = 0x5F;

constexpr int VOLUME_DOWN = 35;  // low while pressed

// ST7701 over MIPI-DSI: 480x800 portrait, 60 Hz at a 30 MHz pixel clock.
constexpr int PANEL_W = 480, PANEL_H = 800;
// The UI's panel, drawn 2x (ui/scale2x).
constexpr int NATIVE_W = PANEL_W / 2, NATIVE_H = PANEL_H / 2;
// Landscape with the keyboard below: wadamesh's boot screen draws (x, y) at
// native (479 - y, x), scale2x rotation 0. Tune on hardware.
constexpr uint8_t PANEL_ROTATION = 0;
constexpr int LCD_RESET = 14;
constexpr int DSI_LANES = 2;
constexpr uint32_t DSI_LANE_MBPS = 500;
constexpr uint32_t DPI_MHZ = 30;
constexpr int HSYNC = 40, HBP = 40, HFP = 30;
constexpr int VSYNC = 16, VBP = 16, VFP = 16;
constexpr int DSI_LDO_CHAN = 3, DSI_LDO_MV = 2500;

// The ESP32-C6 over SDIO (slot 1). Its reset belongs to the coprocessor;
// SDIO_RST is a free expansion pin handed to ESP-Hosted in its place.
constexpr int SDIO_CLK = 17, SDIO_CMD = 16, SDIO_D0 = 18, SDIO_D1 = 19, SDIO_D2 = 20,
              SDIO_D3 = 21, SDIO_RST = 54;

// ES8156 DAC and its I2S.
constexpr uint8_t CODEC_ADDR = 0x08;
constexpr int I2S_MCLK = 30, I2S_BCK = 29, I2S_WS = 31, I2S_DOUT = 28;

// microSD, SDMMC slot 0 (IO_MUX pins).
constexpr int SD_CLK = 43, SD_CMD = 44, SD_D0 = 39, SD_D1 = 40, SD_D2 = 41, SD_D3 = 42;

}  // namespace tanmatsu

#endif
