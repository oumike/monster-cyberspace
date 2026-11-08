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
#include "lvui/views/side_nav_strip.h"
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

// The app's theme (currentTheme(): the saved one, paper on e-paper) on the
// LVGL styles, applied when it differs from the last one this applied: a
// theme picked in Settings shows at once, on every view.
inline void syncTheme() {
  static const Theme* shown = nullptr;
  const Theme& t = currentTheme();
  if (&t == shown) return;
  shown = &t;
  lvui::applyTheme(t);
}

// narrowGrid for a screen's cell grid.
inline bool narrowGrid(const lvui::CellText& t) { return t.cols() < 40; }

// Places obj over cell rows [top, bottom) and columns [left, left + width).
inline void placeCells(lv_obj_t* obj, int left, int top, int width, int bottom) {
  const lvui::Cell c = lvui::cellOf(obj);
  lvui::place(obj, left * c.w, top * c.h, std::max(0, width) * c.w, std::max(0, bottom - top) * c.h);
}

// sideNavWidth for a screen's cell grid: columns of the Up / Select / Down
// strip, 0 on boards without it.
inline int sideNavWidth(const lvui::CellText& t) {
  return board::sideNav() && t.cols() > 0 ? sideNavCols(t.cell().w) : 0;
}

// renderSideNav for LVGL: the strip over cell rows [top, bottom) at the
// right of a grid `cols` wide, `nav` columns (sideNavWidth) wide; hidden at 0.
class SideNavHost {
 public:
  void create(lv_obj_t* parent) {
    host_ = lvui::plainBox(parent);
    strip_.create(host_);
  }
  void sync(int nav, int top, int bottom, int cols) {
    lvui::setHidden(host_, nav <= 0);
    if (nav <= 0) return;
    placeCells(host_, cols - nav, top, nav, bottom);
    strip_.sync();
  }
  lv_obj_t* obj() const { return host_; }

 private:
  lv_obj_t* host_ = nullptr;
  lvui::SideNavStrip strip_;
};

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
