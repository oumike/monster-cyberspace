// SPDX-License-Identifier: GPL-3.0-or-later
// Bring-up as nicolaielectronics mipi_dsi_abstraction's ST7701 panel with
// espressif esp_lcd_st7701 (MIT) does it, without those components: their
// init list goes out over the DBI IO, then the plain DPI panel runs.
#if defined(BOARD_TANMATSU)

#include "hal/tanmatsu/panel_st7701.h"

#include <Arduino.h>
#include <cstring>

#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "hal/tanmatsu/pins_tanmatsu.h"

namespace tanmatsu::panel {
namespace {

esp_ldo_channel_handle_t ldo = nullptr;
esp_lcd_panel_handle_t handle = nullptr;
esp_lcd_panel_io_handle_t dbi = nullptr;
uint16_t* fb = nullptr;

constexpr size_t kRowBytes = size_t(PANEL_W) * 2;

struct Cmd {
  uint8_t cmd;
  uint8_t len;
  uint8_t data[16];
  uint16_t delayMs;
};

// The Tanmatsu's ST7701 init list (dsi_panel_nicolaielectronics_st7701.c).
// E3 is sent with its four bytes (the original's length 5 reads past them).
constexpr Cmd kInit[] = {
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}, 0},
    {LCD_CMD_NORON, 0, {}, 0},
    {0xEF, 1, {0x08}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10}, 0},
    {0xC0, 2, {0x63, 0x00}, 0},
    {0xC1, 2, {0x10, 0x02}, 0},
    {0xC2, 2, {0x37, 0x08}, 0},
    {0xCC, 1, {0x38}, 0},
    {0xB0, 16, {0x40, 0xC9, 0x90, 0x0D, 0x0F, 0x04, 0x00, 0x07, 0x07, 0x1C, 0x04, 0x52, 0x0F, 0xDF, 0x26, 0xCF}, 0},
    {0xB1, 16, {0x40, 0xC9, 0xCF, 0x0C, 0x90, 0x04, 0x00, 0x07, 0x08, 0x1B, 0x06, 0x55, 0x13, 0x62, 0xE7, 0xCF}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11}, 0},
    {0xB0, 1, {0x5D}, 0},
    {0xB1, 1, {0x2D}, 0},
    {0xB2, 1, {0x07}, 0},
    {0xB3, 1, {0x80}, 0},
    {0xB5, 1, {0x08}, 0},
    {0xB7, 1, {0x85}, 0},
    {0xB8, 1, {0x20}, 0},
    {0xB9, 1, {0x10}, 0},
    {0xC1, 1, {0x78}, 0},
    {0xC2, 1, {0x78}, 0},
    {0xD0, 1, {0x88}, 100},
    {0xE0, 3, {0x00, 0x19, 0x02}, 0},
    {0xE1, 11, {0x05, 0xA0, 0x07, 0xA0, 0x04, 0xA0, 0x06, 0xA0, 0x00, 0x44, 0x44}, 0},
    {0xE2, 13, {}, 0},
    {0xE3, 4, {0x00, 0x00, 0x33, 0x33}, 0},
    {0xE4, 2, {0x44, 0x44}, 0},
    {0xE5, 16, {0x0D, 0x31, 0xC8, 0xAF, 0x0F, 0x33, 0xC8, 0xAF, 0x09, 0x2D, 0xC8, 0xAF, 0x0B, 0x2F, 0xC8, 0xAF}, 0},
    {0xE6, 4, {0x00, 0x00, 0x33, 0x33}, 0},
    {0xE7, 2, {0x44, 0x44}, 0},
    {0xE8, 16, {0x0C, 0x30, 0xC8, 0xAF, 0x0E, 0x32, 0xC8, 0xAF, 0x08, 0x2C, 0xC8, 0xAF, 0x0A, 0x2E, 0xC8, 0xAF}, 0},
    {0xEB, 7, {0x02, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x40}, 0},
    {0xEC, 2, {0x3C, 0x00}, 0},
    {0xED, 16, {0xAB, 0x89, 0x76, 0x54, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x45, 0x67, 0x98, 0xBA}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}, 0},
    {LCD_CMD_SLPOUT, 0, {}, 120},
    {LCD_CMD_DISPON, 0, {}, 0},
};

bool fail(const char* stage) {
  Serial.printf("[tanmatsu] ST7701 %s failed\n", stage);
  return false;
}

bool send(uint8_t cmd, const uint8_t* data, size_t n) {
  return esp_lcd_panel_io_tx_param(dbi, cmd, n ? data : nullptr, n) == ESP_OK;
}

}  // namespace

bool begin() {
  esp_ldo_channel_config_t ldoCfg = {};
  ldoCfg.chan_id = DSI_LDO_CHAN;
  ldoCfg.voltage_mv = DSI_LDO_MV;
  if (esp_ldo_acquire_channel(&ldoCfg, &ldo) != ESP_OK) return fail("LDO");

  esp_lcd_dsi_bus_handle_t bus = nullptr;
  esp_lcd_dsi_bus_config_t busCfg = {};
  busCfg.bus_id = 0;
  busCfg.num_data_lanes = DSI_LANES;
  busCfg.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
  busCfg.lane_bit_rate_mbps = DSI_LANE_MBPS;
  if (esp_lcd_new_dsi_bus(&busCfg, &bus) != ESP_OK) return fail("DSI bus");

  esp_lcd_dbi_io_config_t dbiCfg = {};
  dbiCfg.virtual_channel = 0;
  dbiCfg.lcd_cmd_bits = 8;
  dbiCfg.lcd_param_bits = 8;
  if (esp_lcd_new_panel_io_dbi(bus, &dbiCfg, &dbi) != ESP_OK) return fail("DBI IO");

  esp_lcd_dpi_panel_config_t dpiCfg = {};
  dpiCfg.virtual_channel = 0;
  dpiCfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpiCfg.dpi_clock_freq_mhz = DPI_MHZ;
  dpiCfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpiCfg.num_fbs = 1;
  dpiCfg.video_timing.h_size = PANEL_W;
  dpiCfg.video_timing.v_size = PANEL_H;
  dpiCfg.video_timing.hsync_pulse_width = HSYNC;
  dpiCfg.video_timing.hsync_back_porch = HBP;
  dpiCfg.video_timing.hsync_front_porch = HFP;
  dpiCfg.video_timing.vsync_pulse_width = VSYNC;
  dpiCfg.video_timing.vsync_back_porch = VBP;
  dpiCfg.video_timing.vsync_front_porch = VFP;
  dpiCfg.flags.use_dma2d = true;
  if (esp_lcd_new_panel_dpi(bus, &dpiCfg, &handle) != ESP_OK) return fail("DPI panel");

  // Hardware reset (esp_lcd_st7701: low 10 ms, high 10 ms), then the BSP's
  // 100 ms before the init list.
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, LOW);
  delay(10);
  digitalWrite(LCD_RESET, HIGH);
  delay(110);

  // esp_lcd_st7701's own first commands: page 0, MADCTL, COLMOD 16-bit.
  const uint8_t page0[] = {0x77, 0x01, 0x00, 0x00, 0x00}, madctl = 0x00, colmod = 0x55;
  if (!send(0xFF, page0, sizeof page0) || !send(LCD_CMD_MADCTL, &madctl, 1) ||
      !send(LCD_CMD_COLMOD, &colmod, 1))
    return fail("first commands");
  for (const Cmd& c : kInit) {
    if (!send(c.cmd, c.data, c.len)) return fail("init list");
    if (c.delayMs) delay(c.delayMs);
  }
  if (esp_lcd_panel_init(handle) != ESP_OK) return fail("DPI init");
  // The BSP sets the colour mode again once the video stream runs.
  if (!send(LCD_CMD_COLMOD, &colmod, 1)) return fail("COLMOD");

  void* buf = nullptr;
  if (esp_lcd_dpi_panel_get_frame_buffer(handle, 1, &buf) != ESP_OK || !buf) return fail("frame buffer");
  fb = static_cast<uint16_t*>(buf);
  memset(fb, 0, kRowBytes * PANEL_H);
  flush(0, PANEL_H);
  Serial.printf("[tanmatsu] ST7701 up %dx%d\n", PANEL_W, PANEL_H);
  return true;
}

uint16_t* frame() { return fb; }

void flush(int y0, int y1) {
  if (!fb) return;
  if (y0 < 0) y0 = 0;
  if (y1 > PANEL_H) y1 = PANEL_H;
  if (y1 <= y0) return;
  esp_cache_msync(reinterpret_cast<uint8_t*>(fb) + size_t(y0) * kRowBytes, size_t(y1 - y0) * kRowBytes,
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

// The DPI panel has no sleep of its own: DCS through the DBI IO.
void sleep(bool s) {
  if (!dbi) return;
  if (s) {
    send(LCD_CMD_DISPOFF, nullptr, 0);
    send(LCD_CMD_SLPIN, nullptr, 0);
  } else {
    send(LCD_CMD_SLPOUT, nullptr, 0);
    delay(120);
    send(LCD_CMD_DISPON, nullptr, 0);
  }
}

}  // namespace tanmatsu::panel

#endif
