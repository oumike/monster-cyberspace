#pragma once

#include <vector>

#include "ui/home_tiles.h"

namespace rc {

// Buttons laid out in `columns` columns over grid rows [top, bottom) of a
// `cols` wide grid: each kButtonRows tall, with a blank column at each side
// and between columns and a blank row between button rows. scrollRow (the
// first button row shown) is clamped to [0, maxScroll].
constexpr int kButtonRows = 3;

struct ButtonGrid {
  std::vector<TileRect> rects;  // the buttons shown, from index first on
  int first = 0;
  int scrollRow = 0;
  int maxScroll = 0;
};
ButtonGrid buttonGrid(int cols, int top, int bottom, int count, int columns, int scrollRow);

}  // namespace rc
