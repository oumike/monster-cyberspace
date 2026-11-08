#include "lvui/views/menu_overlay.h"

#include <algorithm>

#include "lvui/styles.h"

namespace lvui {

void MenuOverlay::create(lv_obj_t* layer) {
  backdrop_ = plainBox(layer);
  lv_obj_set_size(backdrop_, lv_pct(100), lv_pct(100));
  addTap(backdrop_, this);
  box_ = plainBox(backdrop_);
  addStyle(box_, Role::Screen);
  lv_obj_set_flex_flow(box_, LV_FLEX_FLOW_COLUMN);
  title_ = lineLabel(box_);
  lv_obj_set_width(title_, lv_pct(100));
  list_.create(box_);
  list_.setListId(rc::ListId::Menu);
  lv_obj_set_flex_grow(list_.obj(), 1);
  setHidden(backdrop_, true);
  placed_ = false;
}

void MenuOverlay::setPlace(rc::OverlayPlace p, int bottomPx, int rightReservePx) {
  if (p == place_ && bottomPx == bottomPx_ && rightReservePx == rightPx_) return;
  place_ = p;
  bottomPx_ = bottomPx;
  rightPx_ = rightReservePx;
  placed_ = false;
}

void MenuOverlay::sync(const rc::Overlay& m) {
  setHidden(backdrop_, !m.isOpen());
  if (!m.isOpen()) return;
  lv_obj_move_foreground(backdrop_);
  place(m);
  list_.sync(m.list());
}

// rc::Overlay::render's placements, in cells of the box's font.
void MenuOverlay::place(const rc::Overlay& m) {
  lv_obj_t* layer = lv_obj_get_parent(backdrop_);
  lv_obj_update_layout(layer);
  const int w = lv_obj_get_width(layer), h = lv_obj_get_height(layer);
  const int n = static_cast<int>(m.list().size());
  if (placed_ && n == lastN_ && w == lastW_ && h == lastH_ && m.title() == lastTitle_) return;
  placed_ = true;
  lastN_ = n;
  lastW_ = w;
  lastH_ = h;
  lastTitle_ = m.title();

  const Cell c = cellOf(box_);
  if (c.w <= 0 || c.h <= 0) return;
  const int cols = w / c.w;
  const int bottom = (bottomPx_ < 0 ? h : bottomPx_) / c.h;
  const int reserve = (rightPx_ + c.w - 1) / c.w;
  const int titleLen = static_cast<int>(m.title().size());
  int left = 0, top = 0, width = 0, rows = 0;
  bool barTitle = true;
  switch (place_) {
    case rc::OverlayPlace::BottomRight:
      rows = std::max(0, std::min(n, bottom - 2));
      top = bottom - rows - 1;
      width = std::min(cols - 2, 30);
      left = std::max(1, cols - width - 1 - reserve);
      break;
    case rc::OverlayPlace::Centre:
      width = std::min(cols - 2, std::max(20, titleLen + 2));
      left = std::max(1, (cols - width) / 2);
      top = std::max(1, (bottom - n - 1) / 2);
      rows = std::max(0, std::min(bottom, top + 1 + n) - top - 1);
      break;
    case rc::OverlayPlace::Bottom:
      // The title in the error colour on the page, two rows of choices.
      barTitle = false;
      left = 1;
      top = bottom - 3;
      width = cols - 2;
      rows = 2;
      break;
  }
  lv_obj_set_pos(box_, left * c.w, top * c.h);
  lv_obj_set_size(box_, width * c.w, (rows + 1) * c.h);
  lv_obj_remove_style(title_, style(Role::Bar), 0);
  lv_obj_remove_style(title_, style(Role::Error), 0);
  addStyle(title_, barTitle ? Role::Bar : Role::Error);
  setText(title_, barTitle ? U" " + m.title() : m.title());
}

void MenuOverlay::tapped(lv_obj_t*, bool) {
  rc::KeyEvent e;
  e.action = rc::Action::Back;
  emit(e);
}

}  // namespace lvui
