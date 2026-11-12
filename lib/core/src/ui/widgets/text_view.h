#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
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
  // Source code: no markdown. Unwrapped, long lines run off the right and
  // left()/right() scroll them by kCodeStep columns; wrap folds them. Back to
  // the left edge.
  void setCode(bool wrap);
  void left();
  void right();
  int leftCol() const { return left_; }
  static constexpr int kCodeStep = 8;
  int top() const { return top_; }
  // Scrolled to the last line (or the text fits), as of the last render.
  bool atEnd() const { return top_ >= maxTop(); }
  // Draws rows [top, bottom) in columns [left, left + width).
  void render(Grid& g, int top, int bottom, int left, int width, const Theme& t);

  // A laid-out line as render() draws it.
  struct ShownLine {
    std::u32string_view text;
    uint16_t fg;
    bool bold;
  };
  // For a view drawn without a Grid: lays out and scrolls as render() does
  // for a view `rows` tall and `width` wide, then calls line(r, shown) for
  // each laid-out line on row r. True when a scroll bar takes the last column.
  bool show(int rows, int width, const Theme& t, const std::function<void(int, const ShownLine&)>& line);
  // Laid-out lines in all, as of the last render or show.
  int lineTotal() const { return static_cast<int>(lines_.size()); }

 private:
  bool prepare(int rows, int width);
  struct Line {
    std::u32string text;
    bool bold = false;
    int para = 0;  // the source paragraph, for styles_
  };
  void layout(int width);
  int maxTop() const;
  int maxLeft() const { return std::max(0, longest_ - laidOutWidth_); }

  std::u32string text_;
  std::vector<Style> styles_;
  std::vector<Line> lines_;
  int laidOutWidth_ = 0;  // the wrap width: the view's, less a scrollbar
  int laidOutFor_ = -1;   // the view width and rows it was laid out for
  int laidOutRows_ = -1;
  int rows_ = 0;
  int top_ = 0;
  bool code_ = false, wrap_ = true;
  int left_ = 0;     // code, unwrapped: the first column shown
  int longest_ = 0;  // code: the widest source line, in cells
};

}  // namespace rc
