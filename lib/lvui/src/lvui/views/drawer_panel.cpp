#include "lvui/views/drawer_panel.h"

#include <algorithm>

#include "lvui/styles.h"
#include "ui/nick_color.h"
#include "ui/widgets/scroll_bar.h"
#include "util/utf8.h"

namespace lvui {

void DrawerPanel::create(lv_obj_t* parent) {
  panel_ = plainBox(parent);
  addStyle(panel_, Role::Drawer);
  addStyle(panel_, Role::DrawerEdge);
  // As Drawer::render's setList: a wrapping list that acts on a tap.
  touch_.attach(panel_, this, rc::ListId::Drawer, /*holdToAct*/ false, /*wraps*/ true);
  // The header and list come with the first sync: a closed drawer is only
  // its panel.
  head_ = header_ = close_ = list_ = nullptr;
  rows_.clear();
  inner_ = listRows_ = rowCols_ = first_ = shown_ = 0;
  cell_ = {0, 0};
  lastW_ = lastH_ = -1;
  model_ = nullptr;
}

void DrawerPanel::makeBody() {
  head_ = plainBox(panel_);
  header_ = lineLabel(head_);
  addStyle(header_, Role::Dim);
  close_ = lineLabel(head_);
  lv_label_set_text_static(close_, "\u2715");
  addTap(close_, this);
  list_ = plainBox(panel_);
  bar_.create(list_);
}

void DrawerPanel::setDocked(bool docked) {
  if (docked == docked_) return;
  docked_ = docked;
  lastW_ = -1;
}

// The grid's columns: the edge in column x, text from x + 1; the header in
// the first row, the names below it.
void DrawerPanel::layout() {
  lv_obj_t* parent = lv_obj_get_parent(panel_);
  lv_obj_update_layout(parent);
  const int w = lv_obj_get_content_width(parent), h = lv_obj_get_content_height(parent);
  if (w == lastW_ && h == lastH_) return;
  lastW_ = w;
  lastH_ = h;
  const Cell c = cellOf(panel_);
  if (c.w <= 0 || c.h <= 0) return;
  cell_ = c;
  const int cols = w / c.w;
  const int width = rc::Drawer::width(cols, docked_);
  inner_ = width - 1;
  listRows_ = std::max(0, h / c.h - 1);
  lv_obj_set_size(panel_, width * c.w, h);
  lv_obj_align(panel_, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_pad_left(panel_, c.w, LV_PART_MAIN);
  place(head_, 0, 0, std::max(inner_, 0) * c.w, c.h);
  lv_obj_set_width(header_, std::max(inner_ - 1, 0) * c.w);
  // Docked, ✕ in the second-last column; its target takes the three last.
  const bool close = docked_ && inner_ >= 4;
  setHidden(close_, !close);
  lv_obj_set_width(close_, c.w);
  lv_obj_align(close_, LV_ALIGN_TOP_RIGHT, -c.w, 0);
  lv_obj_set_ext_click_area(close_, c.w);
  place(list_, 0, c.h, std::max(inner_, 0) * c.w, listRows_ * c.h);
}

void DrawerPanel::sync(const rc::Drawer& m) {
  model_ = &m;
  if (!head_) makeBody();
  layout();
  const auto& people = m.people();
  setText(header_, U"here \u00b7 " + rc::utf8ToU32(std::to_string(people.size())));
  const int n = static_cast<int>(people.size());
  const Cell c = cell_;
  if (c.w <= 0 || inner_ <= 0 || listRows_ <= 0) {
    shown_ = 0;
    for (Row& r : rows_) setHidden(r.row, true);
    bar_.sync(0, c, 0, 0, 0, 0);
    return;
  }
  // As Drawer::render: the selection on the last row once it passes the
  // bottom, and a longer list keeps the last column for the scroll bar.
  const bool bar = rc::needsScrollBar(n, listRows_) && inner_ > 3;
  rowCols_ = inner_ - (bar ? 1 : 0);
  first_ = std::max(0, m.selected() - listRows_ + 1);
  shown_ = std::clamp(n - first_, 0, listRows_);
  while (static_cast<int>(rows_.size()) < shown_) {
    Row r;
    r.row = plainBox(list_);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
    addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
    r.name = lineLabel(r.row);
    lv_obj_set_width(r.name, LV_SIZE_CONTENT);
    r.idle = lineLabel(r.row);
    lv_obj_set_width(r.idle, LV_SIZE_CONTENT);
    addStyle(r.idle, Role::Dim);
    rows_.push_back(r);
  }
  const rc::Theme& t = currentTheme();
  for (int k = 0; k < static_cast<int>(rows_.size()); ++k) {
    Row& r = rows_[k];
    setHidden(r.row, k >= shown_);
    if (k >= shown_) continue;
    const int i = first_ + k;
    const rc::Drawer::Person& p = people[i];
    place(r.row, 0, k * c.h, rowCols_ * c.w, c.h);
    const int nameMax = rowCols_ - (p.idle ? 2 : 0);
    setText(r.name, fitCells(rc::utf8ToU32(p.username), nameMax));
    setText(r.idle, p.idle ? std::u32string_view(U" z") : std::u32string_view());
    const bool self = !m.self().empty() && rc::sameUsername(p.username, m.self());
    setTextColor(r.name, rc::nickColor(t, p.username, self));
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  bar_.sync(inner_ * c.w, c, bar ? listRows_ : 0, n, listRows_, first_);
}

// The grid's row targets: the edge column through the last name column.
int DrawerPanel::itemAt(lv_point_t p) const {
  if (!list_ || cell_.w <= 0 || cell_.h <= 0) return -1;
  lv_area_t pa, la;
  lv_obj_get_coords(panel_, &pa);
  lv_obj_get_coords(list_, &la);
  if (p.x < pa.x1 || p.y < la.y1) return -1;
  const int k = (p.y - la.y1) / cell_.h, col = (p.x - pa.x1) / cell_.w;
  if (k >= shown_ || col > rowCols_) return -1;
  return first_ + k;
}

int DrawerPanel::selectedItem() const { return model_ ? model_->selected() : -1; }

int DrawerPanel::itemCount() const { return model_ ? static_cast<int>(model_->people().size()) : 0; }

void DrawerPanel::tapped(lv_obj_t*, bool) {
  rc::KeyEvent e;
  e.action = rc::Action::Activate;
  e.target = static_cast<uint8_t>(rc::ListId::Drawer);
  e.index = rc::Drawer::kCloseIndex;
  emit(e);
}

}  // namespace lvui
