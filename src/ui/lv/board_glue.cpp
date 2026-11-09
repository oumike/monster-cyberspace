#include "ui/lv/board_glue.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdlib>

#include "hal/board.h"
#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"

namespace rc {

namespace {

uint32_t ms() { return millis(); }

[[noreturn]] void assertFailed(const char* file, int line) {
  Serial.printf("\n[lvgl] assert failed at %s:%d\n", file ? file : "?", line);
  Serial.flush();
  abort();
}

}  // namespace

// e-paper boards (board::eink) pack the RGB565 frame to 1 bit in pushRows,
// so nothing here depends on it.
void lvglBegin(bool (*readTouch)(int16_t& x, int16_t& y)) {
  lvui::setAssertHandler(assertFailed);
  lvui::setTickSource(ms);
  lvui::init();
  lv_display_t* d = lvui::displayBegin({board::panelWidth(), board::panelHeight(), board::pushRows});
  if (board::hasTouch()) lvui::pointerBegin(readTouch);
  // The UI task calls lv_timer_handler once a frame: refresh and read on
  // every call, not only when LVGL's own period has run out.
  if (d) {
    if (lv_timer_t* t = lv_display_get_refr_timer(d)) lv_timer_set_period(t, 1);
  }
  for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in))
    if (lv_timer_t* t = lv_indev_get_read_timer(in)) lv_timer_set_period(t, 1);
}

void lvglResize() { lvui::displayResize(board::panelWidth(), board::panelHeight()); }

}  // namespace rc
