// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

#include "hal/tab5/panel_tab5.h"

// The Tab5's touch: the ST7123/ST7121's own controller, or a GT911 beside the
// ILI9881C. Which one answers tells which panel is fitted (M5GFX's detect).
namespace tab5::touch {

// The expanders set up first (TP_RST released). Returns the panel the touch
// controller belongs to.
panel::Kind detect();
// Once the panel is up (the ST7123/ST7121 are TDDI): reads the controller's
// info as M5's drivers do before the first report.
void start();
// True while a finger is down; x, y in the native 720x1280 portrait frame.
bool read(int16_t& x, int16_t& y);

}  // namespace tab5::touch

#endif
