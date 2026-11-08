#include "lvui/views/menu_overlay.h"

#include <algorithm>

#include "lvui/styles.h"
#include "ui/button_grid.h"
#include "ui/text_width.h"
#include "ui/wrap.h"

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
  list_.setMissBack(true);
  lv_obj_set_width(list_.obj(), lv_pct(100));
  lv_obj_set_flex_grow(list_.obj(), 1);
  panel_ = plainBox(backdrop_);
  buttonTitle_ = lv_label_create(backdrop_);
  lv_obj_remove_style_all(buttonTitle_);
  lv_label_set_long_mode(buttonTitle_, LV_LABEL_LONG_CLIP);
  grid_.create(backdrop_, rc::ListId::Menu);
  grid_.setMissBack(true);
  setHidden(panel_, true);
  setHidden(buttonTitle_, true);
  setHidden(grid_.obj(), true);
  setHidden(backdrop_, true);
  placed_ = false;
}

void MenuOverlay::setPlace(rc::OverlayPlace p, int bottomPx, int rightReservePx, int topPx) {
  if (p == place_ && bottomPx == bottomPx_ && rightReservePx == rightPx_ && topPx == topPx_) return;
  place_ = p;
  bottomPx_ = bottomPx;
  rightPx_ = rightReservePx;
  topPx_ = topPx;
  placed_ = false;
  btnLaid_ = false;
}

void MenuOverlay::sync(const rc::Overlay& m) {
  setHidden(backdrop_, !m.isOpen());
  if (!m.isOpen()) return;
  lv_obj_move_foreground(backdrop_);
  const bool buttons = syncButtons(m);
  setHidden(box_, buttons);
  setHidden(grid_.obj(), !buttons);
  if (!buttons) {
    // List mode hides what layoutButtons shows, so the next buttons lay out again.
    btnLaid_ = false;
    setHidden(panel_, true);
    setHidden(buttonTitle_, true);
    place(m);
    list_.sync(m.list());
  }
}

bool MenuOverlay::syncButtons(const rc::Overlay& m) {
  if (!rc::Overlay::confirmButtons() || !(m.isConfirm() || m.isButtons())) return false;
  lv_obj_t* layer = lv_obj_get_parent(backdrop_);
  lv_obj_update_layout(layer);
  const int w = lv_obj_get_width(layer), h = lv_obj_get_height(layer);
  const Cell c = cellOf(backdrop_);
  const auto& items = m.list().items();
  bool same = btnLaid_ && w == btnW_ && h == btnH_ && c.w == btnCell_.w && c.h == btnCell_.h && btnTheme_ == themeSerial() &&
              btnConfirm_ == m.isConfirm() && btnTitle_ == m.title() && btnItems_.size() == items.size();
  for (size_t i = 0; same && i < items.size(); ++i)
    same = btnItems_[i].second == items[i].tone && btnItems_[i].first == items[i].label;
  if (!same) {
    btnLaid_ = true;
    btnW_ = w;
    btnH_ = h;
    btnCell_ = c;
    btnTheme_ = themeSerial();
    btnConfirm_ = m.isConfirm();
    btnTitle_ = m.title();
    btnItems_.clear();
    for (const auto& it : items) btnItems_.emplace_back(it.label, it.tone);
    btnOk_ = layoutButtons(m);
    if (btnOk_) grid_.syncTiles(btnTiles_, nullptr, static_cast<int>(items.size()));
  }
  return btnOk_;
}

// rc::Overlay::render's button layouts (renderButtonMenu, the confirm's), in
// cells of the backdrop's font.
bool MenuOverlay::layoutButtons(const rc::Overlay& m) {
  const rc::Theme& t = currentTheme();
  const Cell c = cellOf(backdrop_);
  if (c.w <= 0 || c.h <= 0) return false;
  const int cols = btnW_ / c.w;
  const int bottom = (bottomPx_ < 0 ? btnH_ : bottomPx_) / c.h;
  const int top = (topPx_ + c.h - 1) / c.h;
  const int reserve = (rightPx_ + c.w - 1) / c.w;
  const auto& items = m.list().items();
  const int n = static_cast<int>(items.size());
  btnTiles_.clear();
  auto addTile = [&](int i, int c0, int r0, int c1, int r1, int col) {
    const rc::ListItem& it = items[i];
    TileGrid::Tile tile{c0 * c.w, r0 * c.h, (c1 - c0 + 1) * c.w, (r1 - r0 + 1) * c.h, i, t.dim};
    const int inW = c1 - c0 - 1;
    tile.texts.push_back(TileGrid::Text{(col - c0) * c.w, (rc::kButtonRows / 2) * c.h,
                                        std::max(0, std::min(inW, c1 - col)) * c.w, it.label,
                                        it.tone == rc::ItemTone::Error ? t.error : t.fg});
    btnTiles_.push_back(std::move(tile));
  };
  auto showTitle = [&](const std::u32string& s, int col, int row, int width, int lines, uint16_t fg) {
    setHidden(buttonTitle_, false);
    setText(buttonTitle_, s);
    setTextColor(buttonTitle_, fg);
    setBgColor(buttonTitle_, t.bg);
    lvui::place(buttonTitle_, col * c.w, row * c.h, width * c.w, lines * c.h);
  };
  if (m.isButtons() && !m.isConfirm()) {
    int labelW = 0;
    for (const auto& it : items) labelW = std::max(labelW, rc::textWidth(it.label));
    const int avail = cols - reserve - 1;
    const int bw = std::min(labelW + 2, avail);
    if (n == 0 || bw < 3) return false;
    const int perRow = 2 * bw + 1 <= avail ? 2 : 1;
    constexpr int kPitch = rc::kButtonRows + 1;
    const int btnTop = bottom - (n + perRow - 1) / perRow * kPitch + 1;
    if (btnTop - 1 < top) return false;
    setHidden(panel_, true);
    showTitle(m.title(), 1, btnTop - 1, std::min(rc::textWidth(m.title()), avail), 1, t.fg);
    for (int i = 0; i < n; ++i) {
      const int c0 = 1 + (i % perRow) * (bw + 1), r0 = btnTop + (i / perRow) * kPitch;
      addTile(i, c0, r0, c0 + bw - 1, r0 + rc::kButtonRows - 1, c0 + 1);
    }
    return true;
  }
  // A confirm: the title wrapped above the buttons, two a row (Cancel left).
  const int btnTop = bottom - (n + 1) / 2 * (rc::kButtonRows + 1);
  const rc::ButtonGrid b = btnTop > top ? rc::buttonGrid(cols, btnTop, bottom, n, 2, 0) : rc::ButtonGrid{};
  if (static_cast<int>(b.rects.size()) != n) return false;
  const int width = std::max(1, cols - 2);
  const std::vector<std::u32string> lines = rc::wrapText(m.title(), width, width);
  const int titleTop = std::max(top, btnTop - 1 - static_cast<int>(lines.size()));
  setHidden(panel_, false);
  setBgColor(panel_, t.bg);
  lvui::place(panel_, 0, titleTop * c.h, cols * c.w, (bottom - titleTop) * c.h);
  std::u32string title;
  int shown = 0;
  for (size_t i = 0; i < lines.size() && titleTop + static_cast<int>(i) < btnTop - 1; ++i, ++shown) {
    if (i) title += U'\n';
    title += lines[i];
  }
  showTitle(title, 1, titleTop, width, std::max(1, shown), t.error);
  for (size_t k = 0; k < b.rects.size(); ++k) {
    const rc::TileRect& r = b.rects[k];
    const int inW = r.c1 - r.c0 - 1;
    const int lw = std::min(inW, static_cast<int>(items[b.first + k].label.size()));
    addTile(b.first + static_cast<int>(k), r.c0, r.r0, r.c1, r.r1, r.c0 + 1 + (inW - lw) / 2);
  }
  return true;
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
