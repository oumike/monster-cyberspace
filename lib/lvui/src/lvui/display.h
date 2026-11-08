#pragma once

#include <lvgl.h>

#include <cstdint>

namespace lvui {

// pushRows sends rows [y0, y1) of the full w x h RGB565 frame to the panel;
// e-paper HALs pack it to 1 bit there, so no thresholding happens here.
struct DisplayConfig {
  int w, h;
  void (*pushRows)(const uint16_t* frame, int y0, int y1);
};

// Creates the LVGL display over one full RGB565 frame (PSRAM on boards that
// have it). LVGL draws dirty areas straight into it (direct mode); after each
// refresh the rows they span go to pushRows in one band.
lv_display_t* displayBegin(const DisplayConfig& cfg);
// The panel turned: new resolution, frame regrown if needed, all redrawn.
void displayResize(int w, int h);
// The current frame (panel size), for screenshots.
const uint16_t* frame();

}  // namespace lvui
