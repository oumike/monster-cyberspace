#include "lvui/views/list_box.h"

#include <algorithm>
#include <cstring>

#include "lvui/input.h"
#include "lvui/styles.h"
#include "src/misc/lv_area_private.h"
#include "ui/widgets/scroll_bar.h"
#include "util/utf8.h"

namespace lvui {

namespace {
bool paintNew = RC_LV_LEAN;
}  // namespace

void ListBox::setPainted(bool on) { paintNew = on; }

void ListBox::create(lv_obj_t* parent) {
  painted_ = paintNew;
  box_ = plainBox(parent);
  rows_.clear();
  lines_now_.clear();
  lines_next_.clear();
  text_.clear();
  nextText_.clear();
  shown_ = first_ = width_ = colW_ = 0;
  cols_ = 1;
  cell_ = {0, 0};
  model_ = nullptr;
  if (painted_) {
    lines_ = paintedBox(box_, this);
  } else {
    lines_ = plainBox(box_);
    // Rows, or with columns cells, across then down.
    lv_obj_set_flex_flow(lines_, LV_FLEX_FLOW_ROW_WRAP);
  }
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
  r.tone = rc::ItemTone::Normal;
  return r;
}

void ListBox::fillRow(Row& r, const rc::ListItem& it) {
  setText(r.marker, it.marker ? std::u32string_view(U"\u2022") : std::u32string_view());
  setText(r.label, it.label);
  // One cell between the label and the right text, one after it.
  setText(r.right, it.right.empty() ? std::u32string() : U" " + it.right + U" ");
  if (it.tone != r.tone) {
    for (lv_obj_t* l : {r.marker, r.label, r.right}) {
      lv_obj_remove_style(l, style(Role::Error), 0);
      lv_obj_remove_style(l, style(Role::Dim), 0);
      if (it.tone == rc::ItemTone::Error) addStyle(l, Role::Error);
      else if (it.tone == rc::ItemTone::Dim) addStyle(l, Role::Dim);
    }
    r.tone = it.tone;
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
  cols_ = std::max(1, m.columns());
  if (height <= 0 || cols <= 0) {
    shown_ = width_ = colW_ = 0;
    for (Row& r : rows_) setHidden(r.row, true);
    if (painted_) syncPainted(items, m.selected());
    bar_.sync(0, cell, 0, 0, 0, 0);
    return;
  }
  // As ListView::render: the selection on the last row once it passes it,
  // and the last column for the bar when the list is longer than the rows.
  // With columns the items fill cells across then down, the last cell of a
  // row taking any spare columns.
  const int total = (n + cols_ - 1) / cols_;
  const int firstRow = std::max(0, m.selected() / cols_ - height + 1);
  first_ = firstRow * cols_;
  const bool bar = rc::needsScrollBar(total, height) && cols > 3;
  width_ = bar ? cols - 1 : cols;
  colW_ = width_ / cols_;
  shown_ = std::clamp(n - first_, 0, height * cols_);
  place(lines_, 0, 0, width_ * cell.w, height * cell.h);
  if (painted_) {
    syncPainted(items, m.selected());
    bar_.sync(bar ? cols * cell.w : 0, cell, bar ? height : 0, total, height, firstRow);
    return;
  }
  while (static_cast<int>(rows_.size()) < shown_) rows_.push_back(makeRow(lines_));
  for (int k = 0; k < static_cast<int>(rows_.size()); ++k) {
    Row& r = rows_[k];
    setHidden(r.row, k >= shown_);
    if (k >= shown_) continue;
    const int i = first_ + k;
    lv_obj_set_width(r.row, cellCols(k % cols_) * cell.w);
    fillRow(r, items[i]);
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  bar_.sync(bar ? cols * cell.w : 0, cell, bar ? height : 0, total, height, firstRow);
}

bool ListBox::sameLine(int k) const {
  const int was = static_cast<int>(lines_now_.size()), now = static_cast<int>(lines_next_.size());
  if (k >= was || k >= now) return k >= was && k >= now;
  const Line &a = lines_next_[k], &b = lines_now_[k];
  return a.marker == b.marker && a.tone == b.tone && a.checked == b.checked &&
         std::strcmp(nextText_.c_str() + a.label, text_.c_str() + b.label) == 0 &&
         std::strcmp(nextText_.c_str() + a.right, text_.c_str() + b.right) == 0;
}

void ListBox::syncPainted(const std::vector<rc::ListItem>& items, int selected) {
  lines_next_.clear();
  nextText_.clear();
  for (int k = 0; k < shown_; ++k) {
    const rc::ListItem& it = items[first_ + k];
    Line l{};
    l.marker = it.marker;
    l.tone = it.tone;
    l.checked = first_ + k == selected && rc::cursorShown();
    l.label = static_cast<uint16_t>(nextText_.size());
    nextText_ += rc::u32ToUtf8(it.label);
    nextText_ += '\0';
    l.right = static_cast<uint16_t>(nextText_.size());
    // One cell between the label and the right text, one after it.
    if (!it.right.empty()) nextText_ += " " + rc::u32ToUtf8(it.right) + " ";
    nextText_ += '\0';
    lines_next_.push_back(l);
  }
  const int rows = static_cast<int>(std::max(lines_now_.size(), lines_next_.size()));
  for (int k = 0; k < rows; ++k) {
    if (sameLine(k)) continue;
    const lv_area_t a = rowArea(k);
    lv_obj_invalidate_area(lines_, &a);
  }
  lines_now_.swap(lines_next_);
  text_.swap(nextText_);
}

int ListBox::cellCols(int col) const { return col == cols_ - 1 ? width_ - col * colW_ : colW_; }

lv_area_t ListBox::rowArea(int i) const {
  if (!painted_) {
    lv_area_t a;
    lv_obj_get_coords(rows_[i].row, &a);
    return a;
  }
  lv_area_t a;
  lv_obj_get_coords(lines_, &a);
  const int col = i % cols_;
  a.x1 += col * colW_ * cell_.w;
  a.x2 = a.x1 + cellCols(col) * cell_.w - 1;
  a.y1 += i / cols_ * cell_.h;
  a.y2 = a.y1 + cell_.h - 1;
  return a;
}

void ListBox::paint(lv_obj_t* box, lv_layer_t* layer) {
  lv_area_t a;
  lv_obj_get_coords(box, &a);
  const lv_font_t* font = lv_obj_get_style_text_font(box, LV_PART_MAIN);
  const rc::Theme& t = currentTheme();
  const int32_t cw = cell_.w, h = cell_.h;
  for (int k = 0; k < static_cast<int>(lines_now_.size()); ++k) {
    const Line& l = lines_now_[k];
    const lv_area_t ra = rowArea(k);
    const int32_t x = ra.x1, w = lv_area_get_width(&ra);
    lv_area_t c;
    if (!lv_area_intersect(&c, &ra, &layer->_clip_area)) continue;
    // As a row (Role::Item, Focused when checked): the outline first, then
    // its marker, its label growing to fill, and its right text at its width.
    if (l.checked) paintRect(layer, ra, color(t.bg), LV_OPA_TRANSP, color(t.selectBg), 1);
    const lv_color_t fg =
        color(l.tone == rc::ItemTone::Error ? t.error : l.tone == rc::ItemTone::Dim ? t.dim : t.fg);
    const char* right = text_.c_str() + l.right;
    const int32_t rightW = right[0] ? textWidth(right, font) : 0;
    const int32_t labelW = std::max<int32_t>(0, w - 2 * cw - rightW);
    if (l.marker) paintLabel(layer, box, "\u2022", x, ra.y1, 2 * cw, h, fg, font, a);
    paintLabel(layer, box, text_.c_str() + l.label, x + 2 * cw, ra.y1, labelW, h, fg, font, a);
    if (right[0]) paintLabel(layer, box, right, x + 2 * cw + labelW, ra.y1, -1, h, fg, font, a);
  }
}

int ListBox::itemAt(lv_point_t p) const {
  if (cell_.w <= 0 || cell_.h <= 0) return -1;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  if (p.x < a.x1 || p.y < a.y1) return -1;
  const int r = (p.y - a.y1) / cell_.h, c = (p.x - a.x1) / cell_.w;
  if (c >= width_ || colW_ <= 0) return -1;
  const int k = r * cols_ + std::min(c / colW_, cols_ - 1);
  if (k >= shown_) return -1;
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
