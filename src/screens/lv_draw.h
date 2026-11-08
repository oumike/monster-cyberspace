#pragma once

// draw.h's helpers for screens drawn with LVGL (lib/lvui views).

#include <lvgl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "app/app_state.h"
#include "app/ui_task.h"
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

// What every LVGL screen's frame depends on, as scalars: the keys and events
// handled (a screen's own state changes only in onKey/onEvent), the status
// bar, the cursor, the theme, the text size and obj's size.
struct FrameInputs {
  uint32_t seq;
  StatusInputs status;
  bool cursor;
  int theme, textSize, w, h;
};
inline bool operator==(const FrameInputs& a, const FrameInputs& b) {
  return a.seq == b.seq && a.status == b.status && a.cursor == b.cursor && a.theme == b.theme &&
         a.textSize == b.textSize && a.w == b.w && a.h == b.h;
}
inline FrameInputs frameInputs(lv_obj_t* obj) {
  const AppState& a = app();
  return FrameInputs{ui::inputSeq(), statusInputs(), cursorShown(), static_cast<int>(a.cfg.theme),
                     static_cast<int>(a.cfg.textSize), static_cast<int>(lv_obj_get_width(obj)),
                     static_cast<int>(lv_obj_get_height(obj))};
}

// An update()'s check before it builds anything: due when obj's frameInputs
// or the screen's own scalars (extra) differ from the last due call's, or once
// a second whatever they are (a safety net for state they miss).
template <size_t N = 0>
class SyncGate {
 public:
  using Extra = std::array<int64_t, N>;
  bool due(lv_obj_t* obj, int64_t nowMs, const Extra& extra = {}) {
    const FrameInputs in = frameInputs(obj);
    if (primed_ && in == last_ && extra == extra_ && nowMs - at_ < kRefreshMs) return false;
    last_ = in;
    extra_ = extra;
    at_ = nowMs;
    primed_ = true;
    return true;
  }

 private:
  static constexpr int64_t kRefreshMs = 1000;
  FrameInputs last_{};
  Extra extra_{};
  int64_t at_ = 0;
  bool primed_ = false;
};

}  // namespace rc
