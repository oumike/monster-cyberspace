#pragma once

// Helpers for the screens, drawn with LVGL (lib/lvui views).

#include <lvgl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "lvui/styles.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/side_nav_strip.h"
#include "lvui/views/view_util.h"
#include "ui/menu_key.h"
#include "util/utf8.h"

namespace rc {

// A UTF-8 message naming the commands key as "Ctrl", with the board's name for it.
inline std::string withMenuKey(std::string_view utf8) {
  return u32ToUtf8(withMenuKey(utf8ToU32(utf8), board::menuKeyName()));
}

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

// sideNavWidth for a screen's cell grid: columns of the Up / Select / Down
// strip, 0 on boards without it.
inline int sideNavWidth(const lvui::CellText& t) {
  return board::sideNav() && t.cols() > 0 ? sideNavCols(t.cell().w) : 0;
}

// renderSideNav for LVGL: the strip over cell rows [top, bottom) at the
// right of a grid `cols` wide, `nav` columns (sideNavWidth) wide; hidden at 0.
// The strip's buttons come with the first sync that shows it, so boards
// without the strip never make them.
class SideNavHost {
 public:
  void create(lv_obj_t* parent) {
    host_ = lvui::plainBox(parent);
    made_ = false;
  }
  void sync(int nav, int top, int bottom, int cols) {
    lvui::setHidden(host_, nav <= 0);
    if (nav <= 0) return;
    if (!made_) {
      strip_.create(host_);
      made_ = true;
    }
    placeCells(host_, cols - nav, top, nav, bottom);
    strip_.sync();
  }
  lv_obj_t* obj() const { return host_; }

 private:
  lv_obj_t* host_ = nullptr;
  lvui::SideNavStrip strip_;
  bool made_ = false;
};

// What every LVGL screen's frame depends on, as scalars: the keys and events
// handled (a screen's own state changes only in onKey/onEvent), the status
// bar, the cursor, the theme, the text size and obj's size. Packed small
// (boards without PSRAM count every byte of internal RAM); a value too big
// for its field leaves ok false.
struct FrameInputs {
  uint32_t seq = 0;
  char32_t conn = 0;
  int32_t minute = 0;
  int16_t bars = 0, battery = 0, w = 0, h = 0;
  uint8_t theme = 0, textSize = 0;
  bool cursor = false, ok = false;
};
inline bool operator==(const FrameInputs& a, const FrameInputs& b) {
  return a.ok && b.ok && a.seq == b.seq && a.conn == b.conn && a.minute == b.minute && a.bars == b.bars &&
         a.battery == b.battery && a.w == b.w && a.h == b.h && a.theme == b.theme &&
         a.textSize == b.textSize && a.cursor == b.cursor;
}
// Stores v in out; false when it doesn't fit.
template <class T>
bool packInto(T& out, int64_t v) {
  out = static_cast<T>(v);
  return static_cast<int64_t>(out) == v;
}
inline FrameInputs frameInputs(lv_obj_t* obj) {
  const AppState& a = app();
  const StatusInputs s = statusInputs();
  FrameInputs f;
  f.seq = ui::inputSeq();
  f.conn = s.conn;
  f.cursor = cursorShown();
  f.ok = packInto(f.minute, s.minute) && packInto(f.bars, s.bars) && packInto(f.battery, s.battery) &&
         packInto(f.w, lv_obj_get_width(obj)) && packInto(f.h, lv_obj_get_height(obj)) &&
         packInto(f.theme, static_cast<int64_t>(a.cfg.theme)) &&
         packInto(f.textSize, static_cast<int64_t>(a.cfg.textSize));
  return f;
}

// An update()'s check before it builds anything: due when obj's frameInputs
// or the screen's own scalars (extra) differ from the last due call's, or once
// a second whatever they are (a safety net for state they miss). Inputs that
// don't fit FrameInputs' fields are always due. Screens keep theirs in their
// Views, so it lives only while the tree is up and a new tree is due at once.
template <size_t N = 0>
class SyncGate {
 public:
  using Extra = std::array<int64_t, N>;
  bool due(lv_obj_t* obj, int64_t nowMs, const Extra& extra = {}) {
    const FrameInputs in = frameInputs(obj);
    if (in == last_ && extra == extra_ && nowMs - at_ < kRefreshMs) return false;
    last_ = in;
    extra_ = extra;
    at_ = nowMs;
    return true;
  }

 private:
  static constexpr int64_t kRefreshMs = 1000;
  FrameInputs last_{};  // ok false until the first due call: unprimed
  Extra extra_{};
  int64_t at_ = 0;
};

}  // namespace rc
