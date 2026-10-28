#include "ui/widgets/text_area.h"

#include <algorithm>

#include "ui/tap_map.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace rc {

bool TextArea::insert(char32_t c) {
  if (text_.size() >= max_) return false;
  text_.insert(text_.begin() + cursor_, c);
  ++cursor_;
  return true;
}

void TextArea::backspace() {
  if (cursor_ == 0) return;
  text_.erase(--cursor_, 1);
}

bool TextArea::left() {
  if (cursor_ == 0) return false;
  --cursor_;
  return true;
}

bool TextArea::right() {
  if (cursor_ >= text_.size()) return false;
  ++cursor_;
  return true;
}

std::string TextArea::utf8() const { return u32ToUtf8(text_); }

void TextArea::clear() {
  text_.clear();
  cursor_ = 0;
}

void TextArea::setCursor(size_t pos) { cursor_ = std::min(pos, text_.size()); }

// Each logical line splits into len / width + 1 segments, so a line that
// exactly fills the width gets an empty segment for the cursor after it.
std::vector<TextArea::Segment> TextArea::segments(int width) const {
  const size_t w = static_cast<size_t>(std::max(width, 1));
  std::vector<Segment> segs;
  size_t start = 0;
  while (true) {
    size_t nl = text_.find(U'\n', start);
    size_t end = nl == std::u32string::npos ? text_.size() : nl;
    for (size_t s = start;;) {
      const std::u32string_view rest(text_.data() + s, end - s);
      const size_t n = rest.empty() ? 0 : std::max<size_t>(fitChars(rest, static_cast<int>(w)), 1);
      segs.push_back({s, n});
      s += n;
      // A segment that ends the line short of the width holds the cursor.
      if (s == end && textWidth(rest.substr(0, n)) < static_cast<int>(w)) break;
      if (s == end) {
        segs.push_back({end, 0});
        break;
      }
    }
    if (nl == std::u32string::npos) break;
    start = nl + 1;
  }
  return segs;
}

size_t TextArea::lineOf(const std::vector<Segment>& segs, size_t pos) {
  size_t i = 0;
  while (i + 1 < segs.size() && segs[i + 1].start <= pos) ++i;
  return i;
}

void TextArea::moveTo(const std::vector<Segment>& segs, size_t from, size_t to) {
  size_t col = cursor_ - segs[from].start;
  const Segment& s = segs[to];
  size_t maxCol = s.len;
  // The end of a full wrapped segment is the start of the next one.
  if (to + 1 < segs.size() && segs[to + 1].start == s.start + s.len && maxCol > 0) --maxCol;
  cursor_ = s.start + std::min(col, maxCol);
}

bool TextArea::up(int width) {
  auto segs = segments(width);
  size_t i = lineOf(segs, cursor_);
  if (i == 0) return false;
  moveTo(segs, i, i - 1);
  return true;
}

bool TextArea::down(int width) {
  auto segs = segments(width);
  size_t i = lineOf(segs, cursor_);
  if (i + 1 >= segs.size()) return false;
  moveTo(segs, i, i + 1);
  return true;
}

void TextArea::render(Grid& g, int top, int bottom, const Theme& t, bool focused) const {
  const int width = g.cols();
  const int height = bottom - top;
  if (height <= 0 || width <= 0) return;
  auto segs = segments(width);
  const size_t cur = lineOf(segs, cursor_);
  const size_t first = cur >= static_cast<size_t>(height) ? cur - height + 1 : 0;
  for (int r = 0; r < height; ++r) {
    const int row = top + r;
    g.fillRow(row, t.inputBg);
    const size_t li = first + r;
    if (li >= segs.size()) continue;
    const Segment& s = segs[li];
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Field;
      tt.field = fieldId_;
      tt.first = static_cast<int16_t>(s.start);
      tt.len = static_cast<int16_t>(s.len);
      activeTaps()->addCells(0, row, width - 1, row, tt);
    }
    g.putText(0, row, std::u32string_view(text_).substr(s.start, s.len), t.fg, t.inputBg, width);
    if (focused && li == cur) {
      int col = textWidth(std::u32string_view(text_).substr(s.start, cursor_ - s.start));
      char32_t cp = cursor_ < text_.size() && text_[cursor_] != U'\n' ? text_[cursor_] : U' ';
      g.put(col, row, cp, t.cursorFg, t.cursorBg);
      if (cellWidth(cp) == 2) g.put(col + 1, row, wideTail(cp), t.cursorFg, t.cursorBg);
    }
  }
}

}  // namespace rc
