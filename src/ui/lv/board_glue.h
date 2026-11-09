#pragma once

#include <cstdint>

namespace rc {

// Starts LVGL on this board's panel and, on a board with touch, a pointer
// read through readTouch (the UI task's sample of the frame, so LVGL and the
// gesture recogniser see the same finger). Called before the UI task runs.
// LVGL's display refresh and pointer read then run on every lv_timer_handler
// call, once per UI frame. A failed LV_ASSERT logs its file and line on the
// console and aborts, so the device restarts instead of hanging.
void lvglBegin(bool (*readTouch)(int16_t& x, int16_t& y));
// The panel turned: follow board::panelWidth/panelHeight.
void lvglResize();

}  // namespace rc
