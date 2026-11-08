#include "lvui/views/input_field.h"

#include <algorithm>

#include "lvui/input.h"
#include "lvui/styles.h"
#include "ui/emoji_set.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace lvui {

void InputField::create(lv_obj_t* parent) {
  row_ = plainBox(parent);
  addStyle(row_, Role::Input);
  lv_obj_set_width(row_, lv_pct(100));
  lv_obj_set_height(row_, cellOf(row_).h);
  emoji_ = lineLabel(row_);
  prompt_ = lineLabel(row_);
  before_ = lineLabel(row_);
  cursor_ = lineLabel(row_);
  addStyle(cursor_, Role::Cursor);
  after_ = lineLabel(row_);
  lv_obj_add_flag(emoji_, LV_OBJ_FLAG_HIDDEN);
  addTap(row_, this);
  laidOut_ = false;
}

void InputField::sync(const rc::InputLine& m, std::u32string_view prompt) {
  const Cell cell = cellOf(row_);
  if (cell.w <= 0 || cell.h <= 0) return;
  if (lv_obj_get_style_height(row_, LV_PART_MAIN) != cell.h) lv_obj_set_height(row_, cell.h);
  lv_obj_update_layout(row_);
  const int cols = lv_obj_get_content_width(row_) / cell.w;
  field_ = m.fieldId();
  const rc::Theme& t = currentTheme();
  const uint16_t fg = m.disabled() ? t.dim : t.fg;
  const bool emojiOn = hasPointer() && m.emojiButton() && cols >= 12;
  if (laidOut_ && cols == cols_ && cell.w == cell_.w && cell.h == cell_.h && m.cursor() == cursorPos_ &&
      m.disabled() == disabled_ && emojiOn == emojiOn_ && fg == fg_ && prompt == promptText_ &&
      text_ == m.text())
    return;
  laidOut_ = true;
  cols_ = cols;
  cell_ = cell;
  cursorPos_ = m.cursor();
  disabled_ = m.disabled();
  emojiOn_ = emojiOn;
  fg_ = fg;
  promptText_.assign(prompt);
  text_ = m.text();
  layout(m, prompt, cols, cell);
}

void InputField::layout(const rc::InputLine& m, std::u32string_view prompt, int cols, Cell cell) {
  const rc::Theme& t = currentTheme();
  int col = 0;
  setHidden(emoji_, !emojiOn_);
  if (emojiOn_) {
    const std::u32string_view e(&rc::kTrayEmoji[0], 1);
    setText(emoji_, e);
    setTextColor(emoji_, t.fg);
    lv_obj_set_pos(emoji_, 0, 0);
    col = std::min(rc::textWidth(e), 2) + 1;
  }
  const size_t pn = rc::fitChars(prompt, std::max(cols - col, 0));
  setText(prompt_, prompt.substr(0, pn));
  setTextColor(prompt_, fg_);
  lv_obj_set_pos(prompt_, col * cell.w, 0);
  col += rc::textWidth(prompt.substr(0, pn));
  const int avail = cols - col;
  fieldCol_ = col;
  avail_ = std::max(avail, 0);
  first_ = len_ = 0;
  if (avail <= 0) {
    for (lv_obj_t* l : {before_, cursor_, after_}) setHidden(l, true);
    return;
  }

  // As InputLine::render: scroll so the text up to the cursor, and the
  // cursor's own cell, fit; then draw what fits from there.
  const std::u32string& text = text_;
  const size_t cursor = cursorPos_;
  auto widthAt = [&](size_t i) { return i < text.size() ? std::max(rc::cellWidth(text[i]), 1) : 1; };
  size_t start = cursor;
  for (int used = widthAt(cursor); start > 0;) {
    const int w = rc::cellWidth(text[start - 1]);
    if (used + w > avail) break;
    used += w;
    --start;
  }
  first_ = start;
  len_ = std::min<size_t>(avail, text.size() - start);

  std::u32string pre, cur, post;
  int preW = 0, curW = 0;
  for (int used = 0, idx = static_cast<int>(start); used < avail; ++idx) {
    const size_t i = static_cast<size_t>(idx);
    if (i >= text.size() && (disabled_ || i != cursor)) break;
    const char32_t cp = i < text.size() ? text[i] : U' ';
    const int w = rc::cellWidth(cp);
    if (w == 0) continue;
    if (used + w > avail) break;
    if (!disabled_ && i == cursor) {
      cur += cp;
      curW = w;
    } else if (disabled_ || i < cursor) {
      pre += cp;
      preW += w;
    } else {
      post += cp;
    }
    used += w;
    if (i >= text.size()) break;  // one blank cell past the end is enough
  }
  setHidden(before_, false);
  setText(before_, pre);
  setTextColor(before_, fg_);
  lv_obj_set_pos(before_, fieldCol_ * cell.w, 0);
  setHidden(cursor_, curW == 0);
  setText(cursor_, cur);
  lv_obj_set_pos(cursor_, (fieldCol_ + preW) * cell.w, 0);
  setHidden(after_, post.empty());
  setText(after_, post);
  setTextColor(after_, fg_);
  lv_obj_set_pos(after_, (fieldCol_ + preW + curW) * cell.w, 0);
}

void InputField::tapped(lv_obj_t*, bool) {
  if (cell_.w <= 0) return;
  lv_area_t a;
  lv_obj_get_content_coords(row_, &a);
  const lv_point_t p = tapPoint();
  const int c = (p.x - a.x1) / cell_.w;
  if (p.x < a.x1) return;
  rc::KeyEvent e;
  if (emojiOn_ && c <= 2) {
    e.action = rc::Action::Emoji;
    emit(e);
    return;
  }
  if (c < fieldCol_ || c >= fieldCol_ + avail_) return;
  e.action = rc::Action::Focus;
  e.index = static_cast<int16_t>(first_ + std::min<size_t>(c - fieldCol_, len_));
  e.target = field_;
  emit(e);
}

}  // namespace lvui
