#include "lvui/views/top_bar.h"

#include <algorithm>

#include "lvui/styles.h"
#include "util/utf8.h"

namespace lvui {

namespace {

lv_obj_t* hitBox(lv_obj_t* bar, Tappable* v) {
  lv_obj_t* o = plainBox(bar);
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  addTap(o, v);
  return o;
}

}  // namespace

void TopBar::create(lv_obj_t* parent) {
  bar_ = plainBox(parent);
  addStyle(bar_, Role::Bar);
  // The touch header: taller, in the big font (set before the labels, which
  // take their font's height), its text centred down it.
  const Header& hd = header();
  if (hd.px > 0) lv_obj_set_style_text_font(bar_, hd.font, LV_PART_MAIN);
  lv_obj_set_width(bar_, lv_pct(100));
  lv_obj_set_height(bar_, hd.px > 0 ? hd.px : cellOf(bar_).h);
  lv_obj_set_flex_flow(bar_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  glyph_ = lineLabel(bar_);
  lv_obj_set_width(glyph_, 2 * cellOf(glyph_).w);
  lv_label_set_text_static(glyph_, "\u2039");
  title_ = lineLabel(bar_);
  lv_obj_set_flex_grow(title_, 1);
  right_ = lineLabel(bar_);
  lv_obj_set_width(right_, LV_SIZE_CONTENT);
  backHit_ = hitBox(bar_, this);
  menuHit_ = hitBox(bar_, this);
  last_ = TopBarData{};
  last_.title = U"\uFFFF";  // differs from any real title: the first sync draws
  lastW_ = -1;
}

void TopBar::sync(const TopBarData& d) {
  // Over the strip the screen root keeps for the touch header.
  const int strip = headerStrip(lv_obj_get_parent(bar_));
  if (lv_obj_get_y_aligned(bar_) != -strip) lv_obj_set_y(bar_, -strip);
  lv_obj_update_layout(bar_);
  const Cell c = cellOf(bar_);
  const int w = lv_obj_get_width(bar_), h = lv_obj_get_height(bar_);
  if (d == last_ && w == lastW_) return;
  last_ = d;
  lastW_ = w;
  const int cols = c.w > 0 ? w / c.w : 0;

  std::u32string right;
  right += d.conn;
  right += U' ';
  static const char32_t kBars[] = {U'\u2582', U'\u2584', U'\u2586', U'\u2588'};
  const int bars = std::clamp(d.wifiBars, 0, 4);
  for (int i = 0; i < 4; ++i) right += i < bars ? kBars[i] : U'\u00b7';
  if (d.batteryPct >= 0) right += U" " + rc::utf8ToU32(std::to_string(std::min(d.batteryPct, 100)) + "%");
  if (!d.clock.empty()) right += U" " + rc::utf8ToU32(d.clock);
  const bool menu = d.touch.on && d.touch.menu;
  if (menu) right += U" \u22ef";
  right += U' ';
  const int rightStart = std::max(0, cols - static_cast<int>(right.size()));
  setText(right_, right);

  const bool back = d.touch.on && d.touch.back;
  const int titleCol = back ? 2 : 0;
  std::u32string left = back ? d.title : U" " + d.title;
  if (d.count >= 0) left += U" " + rc::utf8ToU32(std::to_string(d.count));
  setText(title_, fitCells(left, std::max(0, rightStart - 1 - titleCol)));
  setHidden(glyph_, !back);

  setHidden(backHit_, !back);
  lv_obj_set_size(backHit_, 4 * c.w, h);
  lv_obj_align(backHit_, LV_ALIGN_LEFT_MID, 0, 0);
  setHidden(menuHit_, !menu);
  lv_obj_set_size(menuHit_, 4 * c.w, h);
  lv_obj_align(menuHit_, LV_ALIGN_LEFT_MID, (cols - 4) * c.w, 0);
}

void TopBar::tapped(lv_obj_t* obj, bool hold) {
  rc::KeyEvent e;
  if (obj == backHit_)
    e.action = hold ? rc::Action::OpenHome : rc::Action::Back;
  else
    e.action = rc::Action::Palette;
  emit(e);
}

}  // namespace lvui
