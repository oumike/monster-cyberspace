#include "lvui/views/entry_list_box.h"

#include <algorithm>

#include "lvui/styles.h"

namespace lvui {

void EntryListBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lines_ = plainBox(box_);
  bar_.create(box_);
  touch_.attach(box_, this, rc::ListId::Main, /*holdToAct*/ false, /*wraps*/ true);
  rows_.clear();
  shown_ = pitch_ = width_ = 0;
  cell_ = {0, 0};
  model_ = nullptr;
}

void EntryListBox::styleHead(lv_obj_t* line1) {
  if (head_)
    lv_obj_set_style_text_font(line1, head_, LV_PART_MAIN);
  else
    lv_obj_remove_local_style_prop(line1, LV_STYLE_TEXT_FONT, LV_PART_MAIN);
  lv_obj_set_height(line1, cellOf(line1).h);
}

void EntryListBox::setHeadFont(const lv_font_t* f) {
  if (f == head_) return;
  head_ = f;
  for (Row& r : rows_) styleHead(r.line1);
}

void EntryListBox::sync(const rc::EntryList& m) {
  model_ = &m;
  lv_obj_update_layout(box_);
  const Cell cell = cellOf(box_);
  if (cell.w <= 0 || cell.h <= 0) return;
  cell_ = cell;
  const int rows = lv_obj_get_content_height(box_) / cell.h;
  const int cols = lv_obj_get_content_width(box_) / cell.w;
  width_ = cols - rc::EntryList::kBarCols;
  const auto& entries = m.rows();
  const int n = static_cast<int>(entries.size());
  if (rows <= 0 || width_ <= 0) {
    shown_ = 0;
    for (Row& r : rows_) setHidden(r.row, true);
    bar_.sync(0, cell, 0, 0, 0, 0);
    return;
  }
  // As EntryList::render: whole entries only, a big head row each when set.
  const int bigH = head_ ? lv_font_get_line_height(head_) : 0;
  const int visible = bigH > 0 ? std::max(rows * cell.h / (bigH + cell.h), 1) : std::max(rows / 2, 1);
  pitch_ = (bigH > 0 ? bigH : cell.h) + cell.h;
  const int first = m.window(visible);
  shown_ = std::clamp(n - first, 0, visible);
  place(lines_, 0, 0, width_ * cell.w, visible * pitch_);
  while (static_cast<int>(rows_.size()) < shown_) {
    Row r;
    r.row = plainBox(lines_);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_COLUMN);
    addStyle(r.row, Role::Item);
    addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
    r.line1 = lineLabel(r.row);
    lv_obj_set_width(r.line1, lv_pct(100));
    styleHead(r.line1);
    r.line2 = lineLabel(r.row);
    lv_obj_set_width(r.line2, lv_pct(100));
    r.entry = -1;
    rows_.push_back(r);
  }
  for (int k = 0; k < static_cast<int>(rows_.size()); ++k) {
    Row& r = rows_[k];
    setHidden(r.row, k >= shown_);
    if (k >= shown_) continue;
    r.entry = first + k;
    const rc::FeedRow& fr = entries[r.entry];
    place(r.row, 0, k * pitch_, width_ * cell.w, pitch_);
    setText(r.line1, fr.line1);
    setText(r.line2, fr.line2);
    setTextColor(r.line1, fr.fg1);
    setTextColor(r.line2, fr.fg2);
    setChecked(r.row, r.entry == m.selected() && rc::cursorShown());
  }
  bar_.sync(cols * cell.w, cell, bigH > 0 ? 2 * visible : rows, n, visible, first, bigH);
}

int EntryListBox::itemAt(lv_point_t p) const {
  if (pitch_ <= 0 || cell_.w <= 0) return -1;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  if (p.x < a.x1 || p.y < a.y1) return -1;
  const int k = (p.y - a.y1) / pitch_, c = (p.x - a.x1) / cell_.w;
  if (k >= shown_ || c >= width_) return -1;
  return rows_[k].entry;
}

int EntryListBox::selectedItem() const { return model_ ? model_->selected() : -1; }

int EntryListBox::itemCount() const { return model_ ? static_cast<int>(model_->size()) : 0; }

}  // namespace lvui
