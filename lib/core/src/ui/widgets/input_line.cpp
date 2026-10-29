#include "ui/widgets/input_line.h"

#include <algorithm>

#include "ui/emoji_set.h"
#include "ui/tap_map.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace rc {

static size_t utf8Bytes(char32_t c) {
  return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
}

void InputLine::insert(char32_t c) {
  size_t n = utf8Bytes(c);
  if (bytes_ + n > kMaxBytes) return;
  text_.insert(text_.begin() + cursor_, c);
  ++cursor_;
  bytes_ += n;
}

void InputLine::backspace() {
  if (cursor_ == 0) return;
  --cursor_;
  bytes_ -= utf8Bytes(text_[cursor_]);
  text_.erase(cursor_, 1);
}

bool InputLine::left() {
  if (cursor_ == 0) return false;
  --cursor_;
  return true;
}

bool InputLine::right() {
  if (atEnd()) return false;
  ++cursor_;
  return true;
}

void InputLine::clear() {
  text_.clear();
  cursor_ = bytes_ = 0;
}

void InputLine::setText(std::u32string_view s) {
  clear();
  for (char32_t c : s) insert(c);
}

std::string InputLine::utf8() const { return u32ToUtf8(text_); }

void InputLine::setCursor(size_t pos) { cursor_ = std::min(pos, text_.size()); }

bool InputLine::replace(size_t start, size_t len, std::u32string_view with) {
  if (start > text_.size() || len > text_.size() - start) return false;
  size_t out = 0, in = 0;
  for (size_t i = 0; i < len; ++i) out += utf8Bytes(text_[start + i]);
  for (char32_t c : with) in += utf8Bytes(c);
  if (bytes_ - out + in > kMaxBytes) return false;
  text_.replace(start, len, with);
  bytes_ = bytes_ - out + in;
  if (cursor_ >= start + len) cursor_ = cursor_ - len + with.size();
  else if (cursor_ > start) cursor_ = start + with.size();
  return true;
}

void InputLine::render(Grid& g, int row, const Theme& t, std::u32string_view prompt) const {
  const uint16_t fg = disabled_ ? t.dim : t.fg;
  g.fillRow(row, t.inputBg);
  int col = 0;
  if (emojiButton_ && activeTaps() && g.cols() >= 12) {
    Target b;
    b.kind = TargetKind::Fixed;
    b.tap.action = Action::Emoji;
    activeTaps()->addCells(0, row, 2, row, b);
    col = g.putText(0, row, std::u32string_view(&kTrayEmoji[0], 1), t.fg, t.inputBg, 2) + 1;
  }
  col += g.putText(col, row, prompt, fg, t.inputBg, g.cols() - col);
  int avail = g.cols() - col;
  if (avail <= 0) return;
  // Scroll so the text up to the cursor, and the cursor's own cell, fit.
  auto widthAt = [&](size_t i) { return i < text_.size() ? std::max(cellWidth(text_[i]), 1) : 1; };
  size_t start = cursor_;
  for (int used = widthAt(cursor_); start > 0;) {
    const int w = cellWidth(text_[start - 1]);
    if (used + w > avail) break;
    used += w;
    --start;
  }
  if (activeTaps()) {
    Target tt;
    tt.kind = TargetKind::Field;
    tt.field = fieldId_;
    tt.first = static_cast<int16_t>(start);
    tt.len = static_cast<int16_t>(std::min<size_t>(avail, text_.size() - start));
    activeTaps()->addCells(col, row, col + avail - 1, row, tt);
  }
  for (int used = 0, idx = static_cast<int>(start); used < avail; ++idx) {
    const size_t i = static_cast<size_t>(idx);
    char32_t cp = i < text_.size() ? text_[i] : U' ';
    int w = cellWidth(cp);
    if (w == 0) continue;
    if (used + w > avail) break;
    const bool cur = !disabled_ && i == cursor_;
    const uint16_t f = cur ? t.cursorFg : fg, b = cur ? t.cursorBg : t.inputBg;
    g.put(col + used, row, cp, f, b);
    if (w == 2) g.put(col + used + 1, row, wideTail(cp), f, b);
    used += w;
  }
}

}  // namespace rc
