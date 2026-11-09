#include "lvui/views/log_box.h"

#include <algorithm>
#include <cstring>

#include "lvui/cell_font.h"
#include "lvui/styles.h"
#include "src/misc/lv_area_private.h"
#include "util/utf8.h"

namespace lvui {

namespace {
bool paintNew = RC_LV_LEAN;

int32_t bubbleRadius(Cell cell) { return std::min<int32_t>(cell.h / 2, 6); }

// A bubble row's box, relative to its row's top left: it runs a radius past
// the row on any side the bubble continues, where the row clips it, so only
// the bubble's own first and last rows show rounded corners.
lv_area_t bubbleBox(uint8_t col, uint8_t cols, bool top, bool bottom, Cell cell) {
  const int32_t r = bubbleRadius(cell);
  return lv_area_t{col * cell.w, top ? 0 : -r, (col + cols) * cell.w - 1, cell.h - 1 + (bottom ? 0 : r)};
}

void bubbleDsc(lv_draw_rect_dsc_t& d, bool self, Cell cell) {
  const rc::Theme& t = currentTheme();
  lv_draw_rect_dsc_init(&d);
  d.radius = bubbleRadius(cell);
  if (t.outlined) {
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = color(t.fg);
    d.border_width = 1;
    d.border_opa = LV_OPA_COVER;
  } else {
    d.bg_color = color(rc::bubbleFill(t, self));
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
  }
}

constexpr uint8_t kSelf = static_cast<uint8_t>(rc::DisplayLine::Bubble::Self);
}  // namespace

LogBox::Bub LogBox::bubOf(const rc::DisplayLine* l) {
  Bub b;
  if (!l || l->bubble == rc::DisplayLine::Bubble::None) return b;
  b.kind = static_cast<uint8_t>(l->bubble);
  b.col = l->bubbleCol;
  b.cols = l->bubbleCols;
  b.top = l->bubbleTop;
  b.bottom = l->bubbleBottom;
  return b;
}

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
  p.bub = bubOf(l);
}

bool LogBox::samePainted(const Row& row) const {
  const Row::Painted &a = row.next, &b = row.now;
  if (a.used != b.used || a.mention != b.mention || !(a.bub == b.bub) || a.end - a.text != b.end - b.text)
    return false;
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
    if (row.bub.kind) {
      lv_area_t bb = bubbleBox(row.bub.col, row.bub.cols, row.bub.top, row.bub.bottom, cell_);
      lv_area_move(&bb, ra.x1, ra.y1);
      lv_area_t clip;
      const lv_area_t saved = layer->_clip_area;
      if (lv_area_intersect(&clip, &ra, &saved)) {
        lv_draw_rect_dsc_t d;
        bubbleDsc(d, row.bub.kind == kSelf, cell_);
        d.base.layer = layer;
        layer->_clip_area = clip;
        lv_draw_rect(layer, &d, &bb);
        layer->_clip_area = saved;
      }
    }
    int32_t x = ra.x1;
    for (int i = 0; i < row.used; ++i) {
      const Run& s = runs_[row.run + i];
      x += paintLabel(layer, box, text_.c_str() + s.at, x, ra.y1, -1, h, color(s.fg), s.bold ? bold : font_, ra);
    }
  }
}

void LogBox::fillBubble(Row& row, const Bub& b) {
  if (!b.kind) {
    if (row.bubble && row.bub.kind) setHidden(row.bubble, true);
    row.bub = b;
    return;
  }
  if (!row.bubble) {
    row.bubble = plainBox(row.obj);
    lv_obj_add_flag(row.bubble, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_move_to_index(row.bubble, 0);
  }
  setHidden(row.bubble, false);
  const rc::Theme& t = currentTheme();
  const bool self = b.kind == kSelf;
  const uint16_t fill = rc::bubbleFill(t, self);
  if (!(row.bub == b)) {
    const lv_area_t a = bubbleBox(b.col, b.cols, b.top, b.bottom, cell_);
    place(row.bubble, a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a));
    lv_obj_set_style_radius(row.bubble, bubbleRadius(cell_), LV_PART_MAIN);
  }
  if (!(row.bub == b) || row.bubFill != fill || row.bubOutlined != t.outlined || !row.bub.kind) {
    lv_draw_rect_dsc_t d;
    bubbleDsc(d, self, cell_);
    lv_obj_set_style_bg_color(row.bubble, d.bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row.bubble, d.bg_opa, LV_PART_MAIN);
    lv_obj_set_style_border_color(row.bubble, d.border_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(row.bubble, d.border_width, LV_PART_MAIN);
    lv_obj_set_style_border_opa(row.bubble, LV_OPA_COVER, LV_PART_MAIN);
    row.bubFill = fill;
    row.bubOutlined = t.outlined;
  }
  row.bub = b;
}

bool LogBox::bubbleAt(int r, BubbleShown& out) const {
  const Row& row = rows_[r];
  const Bub& b = painted_ ? row.now.bub : row.bub;
  if (!b.kind) return false;
  out.col = b.col;
  out.cols = b.cols;
  out.top = b.top;
  out.bottom = b.bottom;
  if (painted_) {
    out.fill = rc::bubbleFill(currentTheme(), b.kind == kSelf);
    out.outlined = currentTheme().outlined;
  } else {
    out.fill = lv_color_to_u16(lv_obj_get_style_bg_color(row.bubble, LV_PART_MAIN));
    out.outlined = lv_obj_get_style_border_width(row.bubble, LV_PART_MAIN) > 0;
  }
  return true;
}

void LogBox::fill(Row& row, const rc::DisplayLine* l) {
  fillBubble(row, bubOf(l));
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
    selArea_ = {0, 0, -1, -1};
    setHidden(sel_, true);
    bar_.sync(0, cell, 0, 0, 0, 0);
    return;
  }

  // The selected rows, and those of them in a bubble, which the outline then hugs.
  struct {
    int top = -1, bottom = -1, btop = -1, bbottom = -1, bcol = 0, bcols = 0;
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
      if (s.line && s.line->bubble != rc::DisplayLine::Bubble::None) {
        if (sel.btop < 0 || r < sel.btop) sel.btop = r;
        if (r > sel.bbottom) sel.bbottom = r;
        sel.bcol = s.line->bubbleCol;
        sel.bcols = s.line->bubbleCols;
      }
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
  selArea_ = {0, 0, -1, -1};
  if (outline) {
    if (sel.btop >= 0)
      selArea_ = {sel.bcol, sel.btop, sel.bcol + sel.bcols - 1, sel.bbottom};
    else
      selArea_ = {0, sel.top, width - 1, sel.bottom};
    place(sel_, selArea_.x1 * cell.w, selArea_.y1 * cell.h, lv_area_get_width(&selArea_) * cell.w,
          lv_area_get_height(&selArea_) * cell.h);
  }
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
