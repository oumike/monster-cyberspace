#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ui/home_tiles.h"

namespace rc {

// Buttons laid out in `columns` columns over grid rows [top, bottom) of a
// `cols` wide grid: each kButtonRows tall, with a blank column at each side
// and between columns and a blank row between button rows. scrollRow (the
// first button row shown) is clamped to [0, maxScroll]. The grid's last
// column is always blank, so a scroll bar can go there.
constexpr int kButtonRows = 3;

struct ButtonGrid {
  std::vector<TileRect> rects;  // the buttons shown, from index first on
  int first = 0;
  int scrollRow = 0;
  int maxScroll = 0;
  int visibleRows = 0, totalRows = 0;  // button rows that fit, and in all
};
// height: each button's rows, outline included (kButtonRows for one line of
// text, one more for each extra line).
ButtonGrid buttonGrid(int cols, int top, int bottom, int count, int columns, int scrollRow,
                      int height = kButtonRows);

// `s` on one line no wider than `width` cells: line breaks and runs of
// spaces become one space, the ends are trimmed, and a cut ends in "…".
std::u32string oneLine(std::u32string_view s, int width);

}  // namespace rc
