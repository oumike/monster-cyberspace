#include "lvui/views/cell_text.h"

#include <algorithm>

#include "lvui/styles.h"
#include "ui/text_width.h"
#include "ui/wrap.h"
#include "util/utf8.h"

namespace lvui {

void CellText::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_set_size(box_, lv_pct(100), lv_pct(100));
  labels_.clear();
  used_ = 0;
}

int CellText::cols() const {
  const Cell c = cellOf(box_);
  return c.w > 0 ? lv_obj_get_width(lv_obj_get_parent(box_)) / c.w : 0;
}

int CellText::rows() const {
  const Cell c = cellOf(box_);
  return c.h > 0 ? lv_obj_get_height(lv_obj_get_parent(box_)) / c.h : 0;
}

CellText::Line& CellText::take() {
  if (used_ == static_cast<int>(labels_.size())) labels_.push_back(Line{lineLabel(box_), false});
  return labels_[used_++];
}

int CellText::line(int col, int row, std::u32string_view s, uint16_t fg, int maxCols) {
  return put(col, row, s, fg, maxCols, nullptr);
}

int CellText::put(int col, int row, std::u32string_view s, uint16_t fg, int maxCols, const uint16_t* bg) {
  lv_obj_update_layout(lv_obj_get_parent(box_));
  const int room = cols() - col;
  const int max = maxCols < 0 ? room : std::min(maxCols, room);
  if (max <= 0 || row < 0 || row >= rows()) return 0;
  const std::u32string_view cut = s.substr(0, rc::fitChars(s, max));
  const int w = rc::textWidth(cut);
  Line& l = take();
  const Cell c = cellOf(box_);
  setHidden(l.label, false);
  place(l.label, col * c.w, row * c.h, w * c.w, c.h);
  setText(l.label, cut);
  setTextColor(l.label, fg);
  if (bg) {
    setBgColor(l.label, *bg);
  } else if (l.opaque) {
    lv_obj_set_style_bg_opa(l.label, LV_OPA_TRANSP, LV_PART_MAIN);
  }
  l.opaque = bg != nullptr;
  return w;
}

int CellText::line(int col, int row, std::string_view utf8, uint16_t fg, int maxCols) {
  return line(col, row, rc::utf8ToU32(utf8), fg, maxCols);
}

int CellText::filled(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg) {
  return put(col, row, s, fg, -1, &bg);
}

int CellText::wrapped(int col, int row, std::string_view utf8, uint16_t fg, int stop) {
  lv_obj_update_layout(lv_obj_get_parent(box_));
  const int w = cols() - col, last = stop < 0 ? rows() - 1 : std::min(stop, rows() - 1);
  for (const std::u32string& l : rc::wrapText(rc::utf8ToU32(utf8), w, w)) {
    if (row >= last) break;
    line(col, row++, l, fg);
  }
  return row;
}

void CellText::end() {
  for (size_t i = static_cast<size_t>(used_); i < labels_.size(); ++i) setHidden(labels_[i].label, true);
}

void HintBar::create(lv_obj_t* parent) {
  row_ = plainBox(parent);
  addStyle(row_, Role::Hint);
  label_ = lineLabel(row_);
  lv_obj_set_width(label_, lv_pct(100));
}

void HintBar::sync(std::u32string_view text) {
  lv_obj_t* parent = lv_obj_get_parent(row_);
  lv_obj_update_layout(parent);
  const Cell c = cellOf(row_);
  if (c.w <= 0 || c.h <= 0) return;
  const int cols = lv_obj_get_width(parent) / c.w, rows = lv_obj_get_height(parent) / c.h;
  place(row_, 0, (rows - 1) * c.h, lv_obj_get_width(parent), c.h);
  setText(label_, text.substr(0, rc::fitChars(text, cols)));
}

}  // namespace lvui
