#include "lvui/views/text_box.h"

#include <algorithm>

#include "lvui/cell_font.h"
#include "lvui/styles.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace lvui {

void TextBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_set_size(box_, lv_pct(100), lv_pct(100));
  cursor_ = nullptr;
  bar_.create(box_);
  touch_.attach(box_, this, rc::ListId::Log);
  addTap(box_, this);
  rows_.clear();
  mode_ = Mode::None;
  cols_ = -1;
  cell_ = {0, 0};
  font_ = nullptr;
  areaWidth_ = -1;
  curRow_ = -1;
}

bool TextBox::shape() {
  lv_obj_update_layout(box_);
  const Cell cell = cellOf(box_);
  if (cell.w <= 0 || cell.h <= 0) return false;
  const lv_font_t* f = lv_obj_get_style_text_font(box_, LV_PART_MAIN);
  const int rows = lv_obj_get_content_height(box_) / cell.h;
  const int cols = lv_obj_get_content_width(box_) / cell.w;
  if (rows != static_cast<int>(rows_.size()) || cols != cols_ || cell.w != cell_.w || cell.h != cell_.h ||
      f != font_) {
    for (Row& r : rows_) lv_obj_delete(r.label);
    rows_.clear();
    cols_ = cols;
    cell_ = cell;
    font_ = f;
    areaWidth_ = -1;
    curRow_ = -1;
    for (int r = 0; r < rows; ++r) {
      Row row{lineLabel(box_), {}, 0, false, false};
      place(row.label, 0, r * cell.h, cols * cell.w, cell.h);
      rows_.push_back(std::move(row));
    }
    // Over the rows, under the scroll bar.
    if (cursor_) lv_obj_move_foreground(cursor_);
    lv_obj_move_foreground(bar_.track());
    lv_obj_move_foreground(bar_.thumb());
  }
  return rows > 0 && cols > 0;
}

void TextBox::setRow(int r, std::u32string_view text, uint16_t fg, bool bold) {
  Row& row = rows_[r];
  if (row.text != text) {
    row.text.assign(text);
    lv_label_set_text(row.label, rc::u32ToUtf8(row.text).c_str());
  }
  if (row.fg != fg || !row.coloured) {
    row.fg = fg;
    row.coloured = true;
    lv_obj_set_style_text_color(row.label, color(fg), LV_PART_MAIN);
  }
  if (row.bold != bold) {
    row.bold = bold;
    if (bold)
      lv_obj_set_style_text_font(row.label, boldOf(font_), LV_PART_MAIN);
    else
      lv_obj_remove_local_style_prop(row.label, LV_STYLE_TEXT_FONT, LV_PART_MAIN);
  }
}

void TextBox::sync(rc::TextView& m) {
  if (mode_ == Mode::Area) return;
  mode_ = Mode::View;
  if (!shape()) return;
  const rc::Theme& t = currentTheme();
  const int rows = static_cast<int>(rows_.size());
  int shown = 0;
  const bool bar = m.show(rows, cols_, t, [this, &shown](int r, const rc::TextView::ShownLine& l) {
    setRow(r, l.text, l.fg, l.bold);
    shown = r + 1;
  });
  for (int r = shown; r < rows; ++r) setRow(r, std::u32string_view(), t.fg, false);
  top_ = m.top();
  bar_.sync(bar ? cols_ * cell_.w : 0, cell_, bar ? rows : 0, m.lineTotal(), rows, top_);
}

void TextBox::sync(const rc::TextArea& m, bool focused) {
  if (mode_ == Mode::View) return;
  if (mode_ == Mode::None) {
    mode_ = Mode::Area;
    addStyle(box_, Role::Input);
    cursor_ = lineLabel(box_);
    addStyle(cursor_, Role::Cursor);
    lv_obj_add_flag(cursor_, LV_OBJ_FLAG_HIDDEN);
  }
  if (!shape()) return;
  const rc::Theme& t = currentTheme();
  const int height = static_cast<int>(rows_.size());
  const int width = cols_ - rc::TextArea::kBarCols;
  if (width <= 0) return;
  field_ = m.fieldId();
  const std::u32string& text = m.text();
  if (width != areaWidth_ || text != areaText_) {
    areaText_ = text;
    areaWidth_ = width;
    segs_ = m.lines(width);
  }
  size_t cur = 0;
  while (cur + 1 < segs_.size() && segs_[cur + 1].start <= m.cursor()) ++cur;
  first_ = cur >= static_cast<size_t>(height) ? cur - height + 1 : 0;
  const std::u32string_view all(areaText_);
  for (int r = 0; r < height; ++r) {
    const size_t li = first_ + r;
    if (li < segs_.size())
      setRow(r, all.substr(segs_[li].start, segs_[li].len), t.fg, false);
    else
      setRow(r, std::u32string_view(), t.fg, false);
  }

  int curRow = -1, curCol = 0;
  char32_t cp = U' ';
  if (focused && cur >= first_ && cur < first_ + height) {
    const rc::TextArea::Segment& s = segs_[cur];
    curRow = static_cast<int>(cur - first_);
    curCol = rc::textWidth(all.substr(s.start, m.cursor() - s.start));
    cp = m.cursor() < all.size() && all[m.cursor()] != U'\n' ? all[m.cursor()] : U' ';
  }
  setHidden(cursor_, curRow < 0);
  if (curRow >= 0 && (curRow != curRow_ || curCol != curCol_ || cp != curCp_)) {
    curRow_ = curRow;
    curCol_ = curCol;
    curCp_ = cp;
    lv_label_set_text(cursor_, rc::u32ToUtf8(std::u32string(1, cp)).c_str());
    place(cursor_, curCol * cell_.w, curRow * cell_.h, std::max(rc::cellWidth(cp), 1) * cell_.w, cell_.h);
  }
  bar_.sync(cols_ * cell_.w, cell_, height, static_cast<int>(segs_.size()), height, static_cast<int>(first_));
}

int TextBox::itemAt(lv_point_t p) const {
  if (mode_ != Mode::View || cell_.h <= 0) return -1;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  if (p.y < a.y1 || p.x < a.x1 || p.x >= a.x1 + cols_ * cell_.w) return -1;
  const int r = (p.y - a.y1) / cell_.h;
  return r < rows() ? top_ + r : -1;
}

void TextBox::tapped(lv_obj_t*, bool) {
  if (mode_ != Mode::Area || cell_.h <= 0 || cell_.w <= 0) return;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  const lv_point_t p = tapPoint();
  const int r = (p.y - a.y1) / cell_.h, c = (p.x - a.x1) / cell_.w;
  const size_t li = first_ + r;
  if (p.y < a.y1 || p.x < a.x1 || r >= rows() || c >= cols_ - rc::TextArea::kBarCols || li >= segs_.size()) return;
  rc::KeyEvent e;
  e.action = rc::Action::Focus;
  e.index = static_cast<int16_t>(segs_[li].start + std::min<size_t>(c, segs_[li].len));
  e.target = field_;
  emit(e);
}

}  // namespace lvui
