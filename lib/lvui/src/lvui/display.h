#pragma once

#include <lvgl.h>

#include <cstdint>

namespace lvui {

// pushRows sends rows [y0, y1) of the full w x h RGB565 frame to the panel;
// e-paper HALs pack it to 1 bit there, so no thresholding happens here.
// stripRows 0: one whole frame. Otherwise strip mode, for boards without
// PSRAM: a buffer that many full-width rows tall, pushed a strip at a time
// as a frame shifted back so pushRows still finds row y at frame + y * w
// (it reads only rows [y0, y1)).
struct DisplayConfig {
  int w, h;
  void (*pushRows)(const uint16_t* frame, int y0, int y1);
  int stripRows = 0;
};

// Creates the LVGL display. With a whole RGB565 frame (PSRAM on boards that
// have it) LVGL draws dirty areas straight into it (direct mode) and after
// each refresh the rows they span go to pushRows in one band. In strip mode
// LVGL draws full-width areas a strip at a time (partial mode).
lv_display_t* displayBegin(const DisplayConfig& cfg);
// The panel turned: new resolution, buffer regrown if needed, all redrawn.
void displayResize(int w, int h);
// The current frame (panel size), for screenshots; null in strip mode.
const uint16_t* frame();

}  // namespace lvui
