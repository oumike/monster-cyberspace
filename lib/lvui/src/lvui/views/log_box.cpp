#include "lvui/views/log_box.h"

#include <cstring>

#include "lvui/cell_font.h"
#include "lvui/styles.h"
#include "src/misc/lv_area_private.h"
#include "util/utf8.h"

namespace lvui {

namespace {
bool paintNew = RC_LV_LEAN;
}  // namespace

void LogBox::setPainted(bool on) { paintNew = on; }

void LogBox::create(lv_obj_t* parent) {
  painted_ = paintNew;
  box_ = plainBox(parent);
  lv_obj_set_size(box_, lv_pct(100), lv_pct(100));
  lines_ = painted_ ? paintedBox(box_, this) : plainBox(box_);
  sel_ = plainBox(box_);
  addStyle(sel_, Role::Focused);
  lv_obj_add_flag(sel_, LV_OBJ_FLAG_HIDDEN);
  bar_.create(box_);
  touch_.attach(box_, this, rc::ListId::Log);
  rows_.clear();
  runs_.clear();
  next_.clear();
  text_.clear();
  nextText_.clear();
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
  runs_.clear();
  text_.clear();
  cols_ = cols;
  cell_ = cell;
  font_ = f;
  place(lines_, 0, 0, width * cell.w, rows * cell.h);
  rows_.reserve(rows);
  for (int r = 0; r < rows; ++r) {
    Row row{nullptr, {}, 0, false, -1, {}, {}};
    if (!painted_) {
      row.obj = plainBox(lines_);
      lv_obj_set_flex_flow(row.obj, LV_FLEX_FLOW_ROW);
      place(row.obj, 0, r * cell.h, width * cell.w, cell.h);
    }
    rows_.push_back(std::move(row));
  }
  if (painted_) lv_obj_invalidate(lines_);
}

void LogBox::fillPainted(Row& row, const rc::DisplayLine* l) {
  const int n = l ? static_cast<int>(l->spans.size()) : 0;
  Row::Painted& p = row.next;
  p.run = static_cast<uint16_t>(next_.size());
  p.text = static_cast<uint16_t>(nextText_.size());
  for (int i = 0; i < n; ++i) {
    const rc::Span& s = l->spans[i];
    next_.push_back(Run{static_cast<uint16_t>(nextText_.size()), s.fg, s.bold});
    nextText_ += rc::u32ToUtf8(s.text);
    nextText_ += '\0';
  }
  p.end = static_cast<uint16_t>(nextText_.size());
  p.used = n;
  p.mention = l && l->mentionTint;
}

bool LogBox::samePainted(const Row& row) const {
  const Row::Painted &a = row.next, &b = row.now;
  if (a.used != b.used || a.mention != b.mention || a.end - a.text != b.end - b.text) return false;
  for (int i = 0; i < a.used; ++i) {
    const Run &x = next_[a.run + i], &y = runs_[b.run + i];
    if (x.fg != y.fg || x.bold != y.bold || x.at - a.text != y.at - b.text) return false;
  }
  return nextText_.compare(a.text, a.end - a.text, text_, b.text, b.end - b.text) == 0;
}

void LogBox::paint(lv_obj_t* box, lv_layer_t* layer) {
  lv_area_t a;
  lv_obj_get_coords(box, &a);
  const int32_t w = lv_area_get_width(&a), h = cell_.h;
  const lv_font_t* bold = boldOf(font_);
  for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
    const Row::Painted& row = rows_[r].now;
    const lv_area_t ra{a.x1, a.y1 + r * h, a.x1 + w - 1, a.y1 + (r + 1) * h - 1};
    lv_area_t c;
    if (!lv_area_intersect(&c, &ra, &layer->_clip_area)) continue;
    if (row.mention) paintRect(layer, ra, color(currentTheme().mentionBg), LV_OPA_COVER, lv_color_black(), 0);
    int32_t x = ra.x1;
    for (int i = 0; i < row.used; ++i) {
      const Run& s = runs_[row.run + i];
      x += paintLabel(layer, box, text_.c_str() + s.at, x, ra.y1, -1, h, color(s.fg), s.bold ? bold : font_, ra);
    }
  }
}

void LogBox::fill(Row& row, const rc::DisplayLine* l) {
  const int n = l ? static_cast<int>(l->spans.size()) : 0;
  const lv_font_t* bold = boldOf(font_);
  for (int i = 0; i < n; ++i) {
    const rc::Span& s = l->spans[i];
    if (i == static_cast<int>(row.spans.size())) {
      lv_obj_t* lab = lineLabel(row.obj);
      row.spans.push_back(SpanCell{lab, 0, false});
      lv_obj_set_style_text_color(lab, color(s.fg), LV_PART_MAIN);
      row.spans.back().fg = s.fg;
    }
    SpanCell& c = row.spans[i];
    setHidden(c.label, false);
    // The label's own text is the only copy kept.
    const std::string text = rc::u32ToUtf8(s.text);
    if (std::strcmp(lv_label_get_text(c.label), text.c_str()) != 0) lv_label_set_text(c.label, text.c_str());
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
#if RC_LOW_MEM
  // No spare labels kept hidden for a longer line later.
  for (int i = static_cast<int>(row.spans.size()) - 1; i >= n; --i) lv_obj_delete(row.spans[i].label);
  if (static_cast<int>(row.spans.size()) > n) row.spans.resize(n);
#else
  for (int i = n; i < row.used; ++i) setHidden(row.spans[i].label, true);
#endif
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
  next_.clear();
  nextText_.clear();
  for (Row& row : rows_) row.next = {};
  const int first = m.show(rows, [this, &sel](int r, const rc::LogView::Shown& s) {
    Row& row = rows_[r];
    row.msg = s.msg;
    if (painted_)
      fillPainted(row, s.line);
    else
      fill(row, s.line);
    if (s.selected) {
      if (sel.top < 0 || r < sel.top) sel.top = r;
      if (r > sel.bottom) sel.bottom = r;
    }
  });
  if (painted_) {
    lv_area_t a;
    lv_obj_get_coords(lines_, &a);
    for (int r = 0; r < rows; ++r) {
      Row& row = rows_[r];
      if (!samePainted(row)) {
        const lv_area_t ra{a.x1, a.y1 + r * cell.h, a.x2, a.y1 + (r + 1) * cell.h - 1};
        lv_obj_invalidate_area(lines_, &ra);
      }
      row.now = row.next;
      row.used = row.now.used;
      row.mention = row.now.mention;
    }
    runs_.swap(next_);
    text_.swap(nextText_);
  }

  const bool outline = sel.top >= 0 && rc::cursorShown();
  setHidden(sel_, !outline);
  if (outline) place(sel_, 0, sel.top * cell.h, width * cell.w, (sel.bottom - sel.top + 1) * cell.h);
  bar_.sync(cols * cell.w, cell, rows, m.lineTotal(), rows, first);
}

std::string LogBox::rowText(int r) const {
  std::string s;
  for (int i = 0; i < rows_[r].used; ++i) s += spanText(r, i);
  return s;
}

std::string LogBox::spanText(int r, int i) const {
  const Row& row = rows_[r];
  return painted_ ? std::string(text_.c_str() + runs_[row.now.run + i].at) : lv_label_get_text(row.spans[i].label);
}

uint16_t LogBox::spanFg(int r, int i) const {
  const Row& row = rows_[r];
  return painted_ ? runs_[row.now.run + i].fg : row.spans[i].fg;
}

bool LogBox::spanBold(int r, int i) const {
  const Row& row = rows_[r];
  return painted_ ? runs_[row.now.run + i].bold : row.spans[i].bold;
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
