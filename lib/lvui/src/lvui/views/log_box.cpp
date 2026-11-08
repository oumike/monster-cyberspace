#include "lvui/views/log_box.h"

#include "lvui/cell_font.h"
#include "lvui/styles.h"
#include "util/utf8.h"

namespace lvui {

void LogBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_set_size(box_, lv_pct(100), lv_pct(100));
  lines_ = plainBox(box_);
  sel_ = plainBox(box_);
  addStyle(sel_, Role::Focused);
  lv_obj_add_flag(sel_, LV_OBJ_FLAG_HIDDEN);
  bar_.create(box_);
  touch_.attach(box_, this, rc::ListId::Log);
  rows_.clear();
  cols_ = -1;
  cell_ = {0, 0};
  font_ = nullptr;
}

void LogBox::shape(int rows, int cols, Cell cell) {
  const lv_font_t* f = lv_obj_get_style_text_font(box_, LV_PART_MAIN);
  const int width = cols - rc::LogView::kBarCols;
  if (rows == static_cast<int>(rows_.size()) && cols == cols_ && cell.w == cell_.w && cell.h == cell_.h &&
      f == font_)
    return;
  lv_obj_clean(lines_);
  rows_.clear();
  cols_ = cols;
  cell_ = cell;
  font_ = f;
  place(lines_, 0, 0, width * cell.w, rows * cell.h);
  rows_.reserve(rows);
  for (int r = 0; r < rows; ++r) {
    Row row{plainBox(lines_), {}, 0, false, -1};
    lv_obj_set_flex_flow(row.obj, LV_FLEX_FLOW_ROW);
    place(row.obj, 0, r * cell.h, width * cell.w, cell.h);
    rows_.push_back(std::move(row));
  }
}

void LogBox::fill(Row& row, const rc::DisplayLine* l) {
  const int n = l ? static_cast<int>(l->spans.size()) : 0;
  const lv_font_t* bold = boldOf(font_);
  for (int i = 0; i < n; ++i) {
    const rc::Span& s = l->spans[i];
    if (i == static_cast<int>(row.spans.size())) {
      lv_obj_t* lab = lineLabel(row.obj);
      row.spans.push_back(SpanCell{lab, {}, 0, false});
      lv_obj_set_style_text_color(lab, color(s.fg), LV_PART_MAIN);
      row.spans.back().fg = s.fg;
    }
    SpanCell& c = row.spans[i];
    setHidden(c.label, false);
    if (c.text != s.text) {
      c.text = s.text;
      lv_label_set_text(c.label, rc::u32ToUtf8(c.text).c_str());
    }
    if (c.fg != s.fg) {
      c.fg = s.fg;
      lv_obj_set_style_text_color(c.label, color(s.fg), LV_PART_MAIN);
    }
    if (c.bold != s.bold) {
      c.bold = s.bold;
      if (s.bold)
        lv_obj_set_style_text_font(c.label, bold, LV_PART_MAIN);
      else
        lv_obj_remove_local_style_prop(c.label, LV_STYLE_TEXT_FONT, LV_PART_MAIN);
    }
  }
  for (int i = n; i < row.used; ++i) setHidden(row.spans[i].label, true);
  row.used = n;
  const bool mention = l && l->mentionTint;
  if (mention != row.mention) {
    row.mention = mention;
    if (mention)
      addStyle(row.obj, Role::Mention);
    else
      lv_obj_remove_style(row.obj, style(Role::Mention), 0);
  }
}

void LogBox::sync(const rc::LogView& m) {
  lv_obj_update_layout(box_);
  const Cell cell = cellOf(box_);
  if (cell.w <= 0 || cell.h <= 0) return;
  const int rows = lv_obj_get_content_height(box_) / cell.h;
  const int cols = lv_obj_get_content_width(box_) / cell.w;
  shape(rows, cols, cell);
  const int width = cols - rc::LogView::kBarCols;
  if (rows <= 0 || width <= 0) {
    setHidden(sel_, true);
    bar_.sync(0, cell, 0, 0, 0, 0);
    return;
  }

  struct {
    int top = -1, bottom = -1;
  } sel;
  const int first = m.show(rows, [this, &sel](int r, const rc::LogView::Shown& s) {
    Row& row = rows_[r];
    row.msg = s.msg;
    fill(row, s.line);
    if (s.selected) {
      if (sel.top < 0 || r < sel.top) sel.top = r;
      if (r > sel.bottom) sel.bottom = r;
    }
  });

  const bool outline = sel.top >= 0 && rc::cursorShown();
  setHidden(sel_, !outline);
  if (outline) place(sel_, 0, sel.top * cell.h, width * cell.w, (sel.bottom - sel.top + 1) * cell.h);
  bar_.sync(cols * cell.w, cell, rows, m.lineTotal(), rows, first);
}

std::string LogBox::rowText(int r) const {
  std::string s;
  const Row& row = rows_[r];
  for (int i = 0; i < row.used; ++i) s += lv_label_get_text(row.spans[i].label);
  return s;
}

int LogBox::itemAt(lv_point_t p) const {
  if (cell_.h <= 0 || cell_.w <= 0) return -1;
  lv_area_t a;
  lv_obj_get_content_coords(box_, &a);
  const int r = (p.y - a.y1) / cell_.h, c = (p.x - a.x1) / cell_.w;
  if (p.y < a.y1 || p.x < a.x1 || r >= static_cast<int>(rows_.size()) || c >= cols_ - rc::LogView::kBarCols)
    return -1;
  return rows_[r].msg;
}

int LogBox::shownItems() const {
  int n = 0;
  for (const Row& r : rows_) n += r.msg >= 0;
  return n;
}

}  // namespace lvui
