#include "ui/lv/board_glue.h"

#include <Arduino.h>

#include "hal/board.h"
#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"

namespace rc {

namespace {
uint32_t ms() { return millis(); }
}  // namespace

// e-paper boards (board::eink) pack the RGB565 frame to 1 bit in pushRows,
// so nothing here depends on it.
void lvglBegin() {
  lvui::setTickSource(ms);
  lvui::init();
  lvui::displayBegin({board::panelWidth(), board::panelHeight(), board::pushRows});
  if (board::hasTouch()) lvui::pointerBegin(board::readTouch);
}

void lvglResize() { lvui::displayResize(board::panelWidth(), board::panelHeight()); }

}  // namespace rc
