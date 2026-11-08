#include "lvui/views/list_box.h"

#include <algorithm>

#include "lvui/input.h"
#include "lvui/styles.h"
#include "ui/widgets/scroll_bar.h"

namespace lvui {

void ListBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  rows_.clear();
  shown_ = first_ = width_ = 0;
  cell_ = {0, 0};
  model_ = nullptr;
  lines_ = plainBox(box_);
  lv_obj_set_flex_flow(lines_, LV_FLEX_FLOW_COLUMN);
  bar_.create(box_);
  touch_.attach(box_, this, list_, /*holdToAct*/ false, /*wraps*/ true);
}

void ListBox::setListId(rc::ListId id) {
  list_ = id;
  touch_.setList(id);
}

ListBox::Row ListBox::makeRow(lv_obj_t* parent) {
  Row r;
  r.row = plainBox(parent);
  lv_obj_set_size(r.row, lv_pct(100), cellOf(parent).h);
  lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
  addStyle(r.row, Role::Item);
  addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
  // The outline over the row's cells, as the grid draws it: its border
  // neither grows the row nor moves the text.
  lv_obj_set_style_pad_all(r.row, -1, LV_STATE_CHECKED);
  lv_obj_add_flag(r.row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  r.marker = lineLabel(r.row);
  lv_obj_set_width(r.marker, 2 * cellOf(r.marker).w);
  r.label = lineLabel(r.row);
  lv_obj_set_flex_grow(r.label, 1);
  r.right = lineLabel(r.row);
  lv_obj_set_width(r.right, LV_SIZE_CONTENT);
  r.error = false;
  return r;
}

void ListBox::fillRow(Row& r, const rc::ListItem& it) {
  setText(r.marker, it.marker ? std::u32string_view(U"\u2022") : std::u32string_view());
  setText(r.label, it.label);
  // One cell between the label and the right text, one after it.
  setText(r.right, it.right.empty() ? std::u32string() : U" " + it.right + U" ");
  const bool error = it.tone == rc::ItemTone::Error;
  if (error != r.error) {
    for (lv_obj_t* l : {r.marker, r.label, r.right}) {
      if (error)
        addStyle(l, Role::Error);
      else
        lv_obj_remove_style(l, style(Role::Error), 0);
    }
    r.error = error;
  }
}

void ListBox::sync(const rc::ListView& m) {
  model_ = &m;
  lv_obj_update_layout(box_);
  const Cell cell = cellOf(box_);
  if (cell.w <= 0 || cell.h <= 0) return;
  cell_ = cell;
  const int height = lv_obj_get_content_height(box_) / cell.h;
  const int cols = lv_obj_get_content_width(box_) / cell.w;
  const auto& items = m.items();
  const int n = static_cast<int>(items.size());
  if (height <= 0 || cols <= 0) {
    shown_ = width_ = 0;
    for (Row& r : rows_) setHidden(r.row, true);
    bar_.sync(0, cell, 0, 0, 0, 0);
    return;
  }
  // As ListView::render: the selection on the last row once it passes it,
  // and the last column for the bar when the list is longer than the rows.
  first_ = std::max(0, m.selected() - height + 1);
  const bool bar = rc::needsScrollBar(n, height) && cols > 3;
  width_ = bar ? cols - 1 : cols;
  shown_ = std::clamp(n - first_, 0, height);
  place(lines_, 0, 0, width_ * cell.w, height * cell.h);
  while (static_cast<int>(rows_.size()) < shown_) rows_.push_back(makeRow(lines_));
  for (int k = 0; k < static_cast<int>(rows_.size()); ++k) {
    Row& r = rows_[k];
    setHidden(r.row, k >= shown_);
    if (k >= shown_) continue;
    const int i = first_ + k;
    fillRow(r, items[i]);
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  bar_.sync(bar ? cols * cell.w : 0, cell, bar ? height : 0, n, height, first_);
}

int ListBox::itemAt(lv_point_t p) const {
  if (cell_.w <= 0 || cell_.h <= 0) return -1;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  if (p.x < a.x1 || p.y < a.y1) return -1;
  const int k = (p.y - a.y1) / cell_.h, c = (p.x - a.x1) / cell_.w;
  if (k >= shown_ || c >= width_) return -1;
  return first_ + k;
}

int ListBox::selectedItem() const { return model_ ? model_->selected() : -1; }

int ListBox::itemCount() const { return model_ ? static_cast<int>(model_->size()) : 0; }

void ListBox::missed(bool) {
  if (!missBack_) return;
  rc::KeyEvent e;
  e.action = rc::Action::Back;
  emit(e);
}

}  // namespace lvui
