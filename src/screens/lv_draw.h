#pragma once

// draw.h's helpers for screens drawn with LVGL (lib/lvui views).

#include <lvgl.h>

#include <algorithm>
#include <string_view>

#include "hal/board.h"
#include "lvui/styles.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/view_util.h"
#include "ui/menu_key.h"

namespace rc {

// drawHint: the last row, the commands key named as the board names it.
inline void syncHint(lvui::HintBar& h, std::u32string_view text) {
  h.sync(withMenuKey(text, board::menuKeyName()));
}

// Keyed boards show `keys`; touch boards show `touch`.
inline void syncHint(lvui::HintBar& h, std::u32string_view keys, std::u32string_view touch) {
  syncHint(h, board::hasTouch() ? touch : keys);
}

// narrowGrid for a screen's cell grid.
inline bool narrowGrid(const lvui::CellText& t) { return t.cols() < 40; }

// Places obj over cell rows [top, bottom) and columns [left, left + width).
inline void placeCells(lv_obj_t* obj, int left, int top, int width, int bottom) {
  const lvui::Cell c = lvui::cellOf(obj);
  lvui::place(obj, left * c.w, top * c.h, std::max(0, width) * c.w, std::max(0, bottom - top) * c.h);
}

}  // namespace rc
