// SPDX-License-Identifier: GPL-3.0-or-later
// Bring-up in the order M5GFX's Panel_DSI does it (LDO, DSI bus, DBI IO, DPI
// panel, the panel's init lists, MADCTL and COLMOD, then the video stream),
// with its timings for each of the Tab5's three panels.
#if defined(BOARD_TAB5)

#include "hal/tab5/panel_tab5.h"

#include <Arduino.h>
#include <cstring>

#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "hal/tab5/panel_tab5_init.h"
#include "hal/tab5/pins_tab5.h"

namespace tab5::panel {
namespace {

struct Timing {
  uint32_t laneMbps, dpiMhz;
  uint16_t hsync, hbp, hfp, vsync, vbp, vfp;
  const init::InitList* lists;
  size_t listCount;
};

template <size_t N>
constexpr size_t count(const init::InitList (&)[N]) {
  return N;
}

constexpr Timing kSt7123 = {1040, 80, 2, 40, 40, 2, 8, 220, init::kSt7123, count(init::kSt7123)};
constexpr Timing kSt7121 = {900, 70, 2, 40, 40, 20, 24, 200, init::kSt7121, count(init::kSt7121)};
constexpr Timing kIli9881c = {1040, 80, 40, 140, 40, 4, 20, 20, init::kIli9881c,
                              count(init::kIli9881c)};

esp_ldo_channel_handle_t ldo = nullptr;
esp_lcd_panel_handle_t handle = nullptr;
esp_lcd_panel_io_handle_t dbi = nullptr;
uint16_t* fb = nullptr;

constexpr size_t kRowBytes = size_t(PANEL_W) * 2;
constexpr uint32_t kBacklightMax = (1u << BACKLIGHT_BITS) - 1;

bool fail(const char* stage) {
  Serial.printf("[tab5] panel %s failed\n", stage);
  return false;
}

bool send(uint8_t cmd, const uint8_t* data, size_t n) {
  return esp_lcd_panel_io_tx_param(dbi, cmd, n ? data : nullptr, n) == ESP_OK;
}

bool sendList(const uint8_t* p) {
  while (*p) {
    const uint8_t len = *p++;
    if (!send(p[0], p + 1, len - 1)) return false;
    p += len;
  }
  return true;
}

}  // namespace

const char* kindName(Kind k) {
  switch (k) {
    case Kind::St7121:
      return "ST7121";
    case Kind::Ili9881c:
      return "ILI9881C";
    default:
      return "ST7123";
  }
}

bool begin(Kind kind) {
  const Timing& t = kind == Kind::St7121 ? kSt7121 : kind == Kind::Ili9881c ? kIli9881c : kSt7123;
  ledcAttach(BACKLIGHT, BACKLIGHT_HZ, BACKLIGHT_BITS);
  ledcWrite(BACKLIGHT, 0);  // dark until the picture is up

  esp_ldo_channel_config_t ldoCfg = {};
  ldoCfg.chan_id = DSI_LDO_CHAN;
  ldoCfg.voltage_mv = DSI_LDO_MV;
  if (esp_ldo_acquire_channel(&ldoCfg, &ldo) != ESP_OK) return fail("LDO");

  esp_lcd_dsi_bus_handle_t bus = nullptr;
  esp_lcd_dsi_bus_config_t busCfg = {};
  busCfg.bus_id = 0;
  busCfg.num_data_lanes = DSI_LANES;
  busCfg.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
  busCfg.lane_bit_rate_mbps = t.laneMbps;
  if (esp_lcd_new_dsi_bus(&busCfg, &bus) != ESP_OK) return fail("DSI bus");

  esp_lcd_dbi_io_config_t dbiCfg = {};
  dbiCfg.virtual_channel = 0;
  dbiCfg.lcd_cmd_bits = 8;
  dbiCfg.lcd_param_bits = 8;
  if (esp_lcd_new_panel_io_dbi(bus, &dbiCfg, &dbi) != ESP_OK) return fail("DBI IO");

  esp_lcd_dpi_panel_config_t dpiCfg = {};
  dpiCfg.virtual_channel = 0;
  dpiCfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpiCfg.dpi_clock_freq_mhz = t.dpiMhz;
  dpiCfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpiCfg.num_fbs = 1;
  dpiCfg.video_timing.h_size = PANEL_W;
  dpiCfg.video_timing.v_size = PANEL_H;
  dpiCfg.video_timing.hsync_pulse_width = t.hsync;
  dpiCfg.video_timing.hsync_back_porch = t.hbp;
  dpiCfg.video_timing.hsync_front_porch = t.hfp;
  dpiCfg.video_timing.vsync_pulse_width = t.vsync;
  dpiCfg.video_timing.vsync_back_porch = t.vbp;
  dpiCfg.video_timing.vsync_front_porch = t.vfp;
  dpiCfg.flags.use_dma2d = true;
  if (esp_lcd_new_panel_dpi(bus, &dpiCfg, &handle) != ESP_OK) return fail("DPI panel");

  for (size_t i = 0; i < t.listCount; ++i) {
    if (!sendList(t.lists[i].cmds)) return fail("init list");
    if (t.lists[i].delayMs) delay(t.lists[i].delayMs);
  }
  const uint8_t madctl = 0x00, colmod = 0x55;
  if (!send(LCD_CMD_MADCTL, &madctl, 1) || !send(LCD_CMD_COLMOD, &colmod, 1))
    return fail("MADCTL/COLMOD");
  if (esp_lcd_panel_init(handle) != ESP_OK) return fail("DPI init");

  void* buf = nullptr;
  if (esp_lcd_dpi_panel_get_frame_buffer(handle, 1, &buf) != ESP_OK || !buf)
    return fail("frame buffer");
  fb = static_cast<uint16_t*>(buf);
  memset(fb, 0, kRowBytes * PANEL_H);
  flush(0, PANEL_H);
  Serial.printf("[tab5] %s up %dx%d\n", kindName(kind), PANEL_W, PANEL_H);
  return true;
}

uint16_t* frame() { return fb; }

void flush(int y0, int y1) {
  if (!fb) return;
  if (y0 < 0) y0 = 0;
  if (y1 > PANEL_H) y1 = PANEL_H;
  if (y1 <= y0) return;
  esp_cache_msync(reinterpret_cast<uint8_t*>(fb) + size_t(y0) * kRowBytes,
                  size_t(y1 - y0) * kRowBytes,
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

void backlight(uint8_t level) { ledcWrite(BACKLIGHT, level * kBacklightMax / 255); }

}  // namespace tab5::panel

#endif
