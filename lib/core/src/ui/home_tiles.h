#pragma once

#include <vector>

#include "input/key_event.h"

namespace rc {

// A Home tile's cell bounds, inclusive.
struct TileRect {
  int c0, r0, c1, r1;
};

// Lays out `count` tiles in two columns, row-major, in the rows between the
// status bar (row 0) and the hint (the last row): rows 1..rows-2. The tiles
// take `percent` of that area's width and `heightPercent` (-1: `percent`) of
// its height, rounded to whole cells and centred. Returns an empty vector when
// a tile would be under 3 rows tall or 10 cols wide.
std::vector<TileRect> homeTiles(int cols, int rows, int count, int percent = 100,
                                int heightPercent = -1);

// How many cells of unitPx make `percent` of an areaPx span (rounded, never
// more than fit), and the padding before them that centres them; the rest,
// padPx or one more, goes after.
struct TileSpan {
  int cells, padPx;
};
TileSpan tileSpan(int areaPx, int unitPx, int percent);

// Moves the tile selection by one key in the two-column grid: Up/Down by a
// row, Left/Right within a row. Clamps at the edges; never wraps.
int tileMove(int selected, int count, Action a);

}  // namespace rc
