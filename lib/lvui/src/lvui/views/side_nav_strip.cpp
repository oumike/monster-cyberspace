#include "lvui/views/side_nav_strip.h"

#include "lvui/styles.h"

namespace lvui {

void SideNavStrip::create(lv_obj_t* parent) {
  strip_ = plainBox(parent);
  addStyle(strip_, Role::Screen);
  static const char* const kGlyphs[3] = {"\u25B2", "\u25CF", "\u25BC"};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t* b = plainBox(strip_);
    lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
    addTap(b, this);
    lv_obj_t* g = lv_label_create(b);
    lv_obj_remove_style_all(g);
    lv_label_set_text_static(g, kGlyphs[i]);
    lv_obj_center(g);
    buttons_[i] = b;
  }
  lastW_ = lastH_ = lastCellW_ = dim_ = -1;
}

void SideNavStrip::sync() {
  const uint16_t dim = currentTheme().dim;
  if (dim != dim_) {
    for (lv_obj_t* b : buttons_) lv_obj_set_style_border_color(b, color(dim), LV_PART_MAIN);
    dim_ = dim;
  }
  lv_obj_t* parent = lv_obj_get_parent(strip_);
  lv_obj_update_layout(parent);
  const int w = lv_obj_get_content_width(parent), h = lv_obj_get_content_height(parent);
  const Cell c = cellOf(strip_);
  if (w == lastW_ && h == lastH_ && c.w == lastCellW_) return;
  lastW_ = w;
  lastH_ = h;
  lastCellW_ = c.w;
  if (c.h <= 0) return;
  lv_obj_set_size(strip_, rc::sideNavCols(c.w) * c.w, h);
  lv_obj_align(strip_, LV_ALIGN_TOP_RIGHT, 0, 0);
  // rc::renderSideNav's split, in rows.
  const int rows = h / c.h, edge = rows / 3;
  const int top[3] = {0, edge, rows - edge};
  const int span[3] = {edge, rows - 2 * edge, edge};
  for (int i = 0; i < 3; ++i) {
    lv_obj_set_pos(buttons_[i], 0, top[i] * c.h);
    lv_obj_set_size(buttons_[i], lv_pct(100), span[i] * c.h);
  }
}

void SideNavStrip::tapped(lv_obj_t* obj, bool) {
  static const rc::Action kActs[3] = {rc::Action::Up, rc::Action::Select, rc::Action::Down};
  rc::KeyEvent e;
  for (int i = 0; i < 3; ++i)
    if (obj == buttons_[i]) e.action = kActs[i];
  emit(e);
}

}  // namespace lvui
