#pragma once

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// Read-only scrolling text (the release notes). Understands a little
// markdown: "#" headings lose their marks and are drawn bold, and "- " or
// "* " bullets wrap with a hanging indent. A scrollbar takes the right column
// when the text overflows. On touch boards the rows register as a
// non-wrapping, hold-to-act Log list, so a drag or swipe scrolls it.
class TextView {
 public:
  // A paragraph's colour (and bold), for setText's styles.
  struct Style {
    uint16_t fg;
    bool bold = false;
  };
  void setText(std::u32string text);  // back to the top
  // styles[i] colours the i-th "\n"-separated paragraph; the rest use the
  // theme's fg. Keeps the scroll position unless the text changed.
  // The plain setText drops the styles.
  void setText(std::u32string text, std::vector<Style> styles);
  void up();
  void down();
  void pageUp();
  void pageDown();
  int top() const { return top_; }
  // Draws rows [top, bottom) in columns [left, left + width).
  void render(Grid& g, int top, int bottom, int left, int width, const Theme& t);

 private:
  struct Line {
    std::u32string text;
    bool bold = false;
    int para = 0;  // the source paragraph, for styles_
  };
  void layout(int width);
  int maxTop() const;

  std::u32string text_;
  std::vector<Style> styles_;
  std::vector<Line> lines_;
  int laidOutWidth_ = 0;  // the wrap width: the view's, less a scrollbar
  int laidOutFor_ = -1;   // the view width and rows it was laid out for
  int laidOutRows_ = -1;
  int rows_ = 0;
  int top_ = 0;
};

}  // namespace rc
