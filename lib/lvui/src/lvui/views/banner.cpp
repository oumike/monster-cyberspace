#include "lvui/views/banner.h"

#include "lvui/styles.h"

namespace lvui {

void Banner::create(lv_obj_t* parent) {
  row_ = plainBox(parent);
  addStyle(row_, Role::Hint);
  lv_obj_set_width(row_, lv_pct(100));
  lv_obj_set_height(row_, cellOf(row_).h);
  label_ = lineLabel(row_);
  lv_obj_set_width(label_, lv_pct(100));
  addTap(row_, this);
  setHidden(row_, true);
  box_ = -1;
}

void Banner::sync(const rc::Banner& m, int64_t nowMs) {
  const bool shown = m.visible(nowMs);
  setHidden(row_, !shown);
  if (!shown) return;
  setText(label_, U" " + m.text());
  const rc::Theme& t = currentTheme();
  // Outlined themes box an error rather than filling it (rc::Banner::render).
  const bool fill = m.error() && !t.outlined;
  const uint16_t bg = fill ? t.error : t.hintBg;
  const uint16_t fg = fill ? t.bg : m.error() ? t.error : t.hintFg;
  const int8_t box = m.error() && t.outlined ? 1 : 0;
  if (bg != bg_ || box_ < 0) lv_obj_set_style_bg_color(row_, color(bg), LV_PART_MAIN);
  setTextColor(row_, fg);
  if (box != box_ || (box && fg != fg_)) {
    lv_obj_set_style_border_width(row_, box, LV_PART_MAIN);
    lv_obj_set_style_border_color(row_, color(t.error), LV_PART_MAIN);
  }
  bg_ = bg;
  fg_ = fg;
  box_ = box;
}

void Banner::tapped(lv_obj_t*, bool hold) {
  if (!hold && onHide_) onHide_();
}

}  // namespace lvui
