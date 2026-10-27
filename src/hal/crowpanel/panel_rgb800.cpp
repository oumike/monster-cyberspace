#if defined(BOARD_CROWPANEL_50)

#include "hal/crowpanel/panel_rgb800.h"

#include <Arduino.h>
#include <esp_cache.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#include <cstring>

#include "hal/crowpanel/pins_crowpanel.h"

// Timings from camillia-mt's crowpanel_50_display.h (checked on the board).

namespace rgb800 {
namespace {

using namespace crowpanel::p50;

esp_lcd_panel_handle_t panel = nullptr;
uint16_t* fb = nullptr;
constexpr size_t kFbBytes = (size_t)PANEL_W * PANEL_H * 2;

void syncAll() { esp_cache_msync(fb, kFbBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED); }

}  // namespace

bool begin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = PCLK_HZ;
  cfg.timings.h_res = PANEL_W;
  cfg.timings.v_res = PANEL_H;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch = 8;
  cfg.timings.vsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch = 8;
  // LovyanGFX's hsync/vsync_polarity 0 is sync idle low, and its
  // pclk_idle_high sets the out edge (esp_lcd's pclk_active_neg), not the
  // idle level. Inverted syncs leave the panel cycling its self-test colours.
  cfg.timings.flags.hsync_idle_low = 1;
  cfg.timings.flags.vsync_idle_low = 1;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  cfg.dma_burst_size = 64;
  cfg.hsync_gpio_num = PIN_HSYNC;
  cfg.vsync_gpio_num = PIN_VSYNC;
  cfg.de_gpio_num = PIN_DE;
  cfg.pclk_gpio_num = PIN_PCLK;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = PIN_DATA[i];
  cfg.flags.fb_in_psram = 1;
  if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) {
    Serial.println("[rgb800] panel create failed");
    return false;
  }
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  void* buf = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &buf) != ESP_OK || !buf) {
    Serial.println("[rgb800] no framebuffer");
    return false;
  }
  fb = static_cast<uint16_t*>(buf);
  clear();
  return true;
}

void clear() {
  if (!fb) return;
  memset(fb, 0, kFbBytes);
  syncAll();
}

// Frame pixel (x, y) at rotation r lands in the framebuffer at:
//   r0 (x, y)   r1 (y, H-1-x)   r2 (W-1-x, H-1-y)   r3 (W-1-y, x)
// with W x H the panel's native 800x480.
void pushRows(const uint16_t* frame, int frameW, int y0, int y1, uint8_t r) {
  if (!fb || y1 <= y0) return;
  r &= 3;
  if (r == 0) {
    memcpy(fb + (size_t)y0 * PANEL_W, frame + (size_t)y0 * frameW, (size_t)(y1 - y0) * PANEL_W * 2);
    esp_cache_msync(fb + (size_t)y0 * PANEL_W, (size_t)(y1 - y0) * PANEL_W * 2,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    return;
  }
  if (r == 2) {
    for (int y = y0; y < y1; ++y) {
      const uint16_t* src = frame + (size_t)y * frameW;
      uint16_t* dst = fb + (size_t)(PANEL_H - 1 - y) * PANEL_W + (PANEL_W - 1);
      for (int x = 0; x < PANEL_W; ++x) *dst-- = src[x];
    }
    esp_cache_msync(fb + (size_t)(PANEL_H - y1) * PANEL_W, (size_t)(y1 - y0) * PANEL_W * 2,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    return;
  }
  // Portrait: a frame row is a framebuffer column.
  for (int y = y0; y < y1; ++y) {
    const uint16_t* src = frame + (size_t)y * frameW;
    if (r == 1) {
      uint16_t* dst = fb + (size_t)(PANEL_H - 1) * PANEL_W + y;
      for (int x = 0; x < frameW; ++x, dst -= PANEL_W) *dst = src[x];
    } else {
      uint16_t* dst = fb + (PANEL_W - 1 - y);
      for (int x = 0; x < frameW; ++x, dst += PANEL_W) *dst = src[x];
    }
  }
  syncAll();
}

}  // namespace rgb800

#endif
