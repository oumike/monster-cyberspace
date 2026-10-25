#include "ui/widgets/input_line.h"

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

void InputLine::render(Grid& g, int row, const Theme& t, std::u32string_view prompt) const {
  const uint16_t fg = disabled_ ? t.dim : t.fg;
  g.fillRow(row, t.inputBg);
  int col = g.putText(0, row, prompt, fg, t.inputBg, g.cols());
  int avail = g.cols() - col;
  if (avail <= 0) return;
  size_t start = cursor_ >= static_cast<size_t>(avail) ? cursor_ - avail + 1 : 0;
  for (int i = 0; i < avail; ++i) {
    size_t idx = start + i;
    char32_t cp = idx < text_.size() ? text_[idx] : U' ';
    if (!disabled_ && idx == cursor_)
      g.put(col + i, row, cp, t.cursorFg, t.cursorBg);
    else
      g.put(col + i, row, cp, fg, t.inputBg);
  }
}

}  // namespace rc
