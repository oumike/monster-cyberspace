#include "lvui/display.h"

#include <cstdlib>

namespace lvui {

namespace {

lv_display_t* disp = nullptr;
void (*push)(const uint16_t*, int, int) = nullptr;
uint16_t* frameBuf = nullptr;
size_t frameBytes = 0;
int bandY0 = -1;
int bandY1 = -1;

// PSRAM boards (and the host) take the frame from LVGL's allocator; without
// PSRAM it comes from the internal heap so it doesn't use up LVGL's budget.
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM && !defined(BOARD_HAS_PSRAM)
void* frameAlloc(size_t n) { return std::malloc(n); }
void frameFree(void* p) { std::free(p); }
#else
void* frameAlloc(size_t n) { return lv_malloc(n); }
void frameFree(void* p) { lv_free(p); }
#endif

// Panels push whole rows, so one band over every dirty area of a refresh
// costs one transfer instead of one per area (and no row goes twice).
void flush(lv_display_t* d, const lv_area_t* a, uint8_t*) {
  if (bandY0 < 0 || a->y1 < bandY0) bandY0 = a->y1;
  if (a->y2 + 1 > bandY1) bandY1 = a->y2 + 1;
  if (lv_display_flush_is_last(d)) {
    if (push) push(frameBuf, bandY0, bandY1);
    bandY0 = bandY1 = -1;
  }
  lv_display_flush_ready(d);
}

bool fit(int w, int h) {
  const size_t need = static_cast<size_t>(w) * h * sizeof(uint16_t);
  if (need > frameBytes) {
    void* p = frameAlloc(need);
    if (!p) return false;
    frameFree(frameBuf);
    frameBuf = static_cast<uint16_t*>(p);
    frameBytes = need;
  }
  return true;
}

void setBuffers(int w) {
  lv_display_set_buffers_with_stride(disp, frameBuf, nullptr, frameBytes, w * sizeof(uint16_t),
                                     LV_DISPLAY_RENDER_MODE_DIRECT);
}

}  // namespace

lv_display_t* displayBegin(const DisplayConfig& cfg) {
  if (disp) return disp;
  if (!fit(cfg.w, cfg.h)) return nullptr;
  disp = lv_display_create(cfg.w, cfg.h);
  if (!disp) return nullptr;
  push = cfg.pushRows;
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  setBuffers(cfg.w);
  lv_display_set_flush_cb(disp, flush);
  return disp;
}

void displayResize(int w, int h) {
  if (!disp) return;
  if (w == lv_display_get_horizontal_resolution(disp) &&
      h == lv_display_get_vertical_resolution(disp))
    return;
  if (!fit(w, h)) return;
  lv_display_set_resolution(disp, w, h);
  setBuffers(w);
  lv_obj_invalidate(lv_display_get_screen_active(disp));
}

const uint16_t* frame() { return frameBuf; }

}  // namespace lvui
