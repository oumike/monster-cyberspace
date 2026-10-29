#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// Multi-line editor for the Composer body. Lines break at '\n' and wrap at
// the render width (hard wrap, one cell per code point).
class TextArea {
 public:
  explicit TextArea(size_t maxChars = 4096) : max_(maxChars) {}
  bool insert(char32_t c);  // refuses past maxChars (counted in code points)
  void backspace();
  bool left();
  bool right();
  bool up(int width);    // false on the first visual line
  bool down(int width);  // false on the last visual line
  size_t length() const { return text_.size(); }
  const std::u32string& text() const { return text_; }
  size_t cursor() const { return cursor_; }
  // Swaps text_[start, start + len) for `with`; a cursor past the range moves
  // with it. False (and no change) out of range or past maxChars.
  bool replace(size_t start, size_t len, std::u32string_view with);
  std::string utf8() const;
  void clear();
  // Moves the cursor to `pos`, clamped to the text length.
  void setCursor(size_t pos);
  // The screen's field id, carried in each tap Field target.
  void setFieldId(uint8_t id) { fieldId_ = id; }
  // Draws rows [top, bottom) across the grid width, keeping the cursor visible.
  void render(Grid& g, int top, int bottom, const Theme& t, bool focused) const;

 private:
  struct Segment {
    size_t start, len;
  };
  std::vector<Segment> segments(int width) const;
  static size_t lineOf(const std::vector<Segment>& segs, size_t pos);
  void moveTo(const std::vector<Segment>& segs, size_t from, size_t to);

  std::u32string text_;
  size_t cursor_ = 0, max_;
  uint8_t fieldId_ = 0;
};

}  // namespace rc
