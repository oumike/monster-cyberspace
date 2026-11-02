// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/RM69A10Display.cpp.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/panel_rm69a10.h"

#include <Arduino.h>
#include <cstring>

#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "hal/tdisplay_p4/esp_lcd_rm69a10.h"
#include "hal/tdisplay_p4/io_xl9535.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::panel {
namespace {

esp_ldo_channel_handle_t ldo = nullptr;
esp_lcd_panel_handle_t handle = nullptr;
esp_lcd_panel_io_handle_t dbi = nullptr;  // DCS commands: brightness
uint16_t* fb = nullptr;
uint8_t level = 255;

constexpr size_t kRowBytes = size_t(PANEL_W) * 2;

bool fail(const char* stage) {
  Serial.printf("[tdp4] RM69A10 %s failed\n", stage);
  return false;
}

}  // namespace

bool begin() {
  // 1. The DSI PHY's LDO, before the DSI bus.
  esp_ldo_channel_config_t ldoCfg = {};
  ldoCfg.chan_id = DSI_LDO_CHAN;
  ldoCfg.voltage_mv = DSI_LDO_MV;
  if (esp_ldo_acquire_channel(&ldoCfg, &ldo) != ESP_OK) return fail("LDO");
  delay(100);

  // 2. Panel reset through the XL9535, after the LDO and before the bus:
  // high, low, high with 200 ms each (shorter, or later, keeps it dark).
  xl9535::set(EXP_SCREEN_RST, true);
  delay(200);
  xl9535::set(EXP_SCREEN_RST, false);
  delay(200);
  xl9535::set(EXP_SCREEN_RST, true);
  delay(200);

  // 3. DSI bus, DBI command IO, DPI video.
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

  // 4. The RM69A10 on top of the DPI panel; its reset is ours (-1).
  rm69a10_vendor_config_t vendor = {};
  vendor.mipi_config.dsi_bus = bus;
  vendor.mipi_config.dpi_config = &dpiCfg;
  vendor.mipi_config.lane_num = DSI_LANES;
  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = -1;
  dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  dev.bits_per_pixel = 16;
  dev.vendor_config = &vendor;
  if (esp_lcd_new_panel_rm69a10(dbi, &dev, &handle) != ESP_OK) return fail("new panel");
  esp_lcd_panel_reset(handle);
  if (esp_lcd_panel_init(handle) != ESP_OK) return fail("init");

  // 5. Blank the frame buffer (fresh PSRAM, garbage) before display on.
  void* buf = nullptr;
  if (esp_lcd_dpi_panel_get_frame_buffer(handle, 1, &buf) != ESP_OK || !buf) return fail("frame buffer");
  fb = static_cast<uint16_t*>(buf);
  memset(fb, 0, kRowBytes * PANEL_H);
  flush(0, PANEL_H);
  esp_lcd_panel_disp_on_off(handle, true);

  // 6. Brightness after display on: the init list's value alone isn't kept.
  brightness(level);
  Serial.printf("[tdp4] RM69A10 up %dx%d\n", PANEL_W, PANEL_H);
  return true;
}

uint16_t* frame() { return fb; }

void flush(int ny0, int ny1) {
  if (!fb) return;
  if (ny0 < 0) ny0 = 0;
  if (ny1 > PANEL_H) ny1 = PANEL_H;
  if (ny1 <= ny0) return;
  // Rows are 1136 bytes, not cache-line multiples.
  esp_cache_msync(reinterpret_cast<uint8_t*>(fb) + size_t(ny0) * kRowBytes, size_t(ny1 - ny0) * kRowBytes,
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

void brightness(uint8_t l) {
  level = l;
  if (dbi) esp_lcd_panel_io_tx_param(dbi, 0x51, &l, 1);  // DCS SET_DISPLAY_BRIGHTNESS
}

void sleep(bool s) {
  if (!handle) return;
  if (s) {
    esp_lcd_panel_disp_on_off(handle, false);
    esp_lcd_panel_disp_sleep(handle, true);
  } else {
    esp_lcd_panel_disp_sleep(handle, false);
    esp_lcd_panel_disp_on_off(handle, true);
    brightness(level);
  }
}

}  // namespace tdp4::panel

#endif
