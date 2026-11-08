#pragma once

namespace rc {

// Starts LVGL on this board's panel (and touch, if it has one). Called from
// ui_task's thread.
void lvglBegin();
// The panel turned: follow board::panelWidth/panelHeight.
void lvglResize();

}  // namespace rc
