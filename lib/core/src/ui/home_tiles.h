#pragma once

#include <utility>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"

namespace rc {

// A Home tile's cell bounds, inclusive.
struct TileRect {
  int c0, r0, c1, r1;
};

// The Home tiles of a touch board: `count` tiles in two columns, row-major,
// taking `percent` of the panel's width and of the areaPx between the status
// bar (row 0) and the hint (the last of gridRows), in cells of `cell`.
// Centred to the pixel: row 1 is a blank pad row above the tiles, and the
// tile rows are drawn shiftPx to the right. A blank cell column separates the
// columns, and a pad row as tall as a cell is wide separates the tile rows.
// With gaps false the tile rows touch, for a grid with no rows to spare for
// the pads. tiles is empty when a tile would be under 3 rows tall or 10 cols
// wide. With spanFirst the first tile is a row of its own, as wide as both
// columns, and the rest follow two to a row.
struct HomeLayout {
  std::vector<TileRect> tiles;
  std::vector<std::pair<int, int>> pads;  // (grid row, px), row 1 first
  int lastRow = 0;                        // rows after it are unused
  int shiftPx = 0;
};
HomeLayout homeLayout(int panelW, int areaPx, int gridRows, CellSize cell, int count,
                      int percent, bool gaps = true, bool spanFirst = false);

// How many cells of unitPx make `percent` of an areaPx span (rounded, never
// more than fit), and the padding before them that centres them; the rest,
// padPx or one more, goes after.
struct TileSpan {
  int cells, padPx;
};
TileSpan tileSpan(int areaPx, int unitPx, int percent);

// Moves the tile selection by one key in the two-column grid: Up/Down by a
// row, Left/Right within a row. Clamps at the edges; never wraps. With
// spanFirst (see homeLayout) Up from either tile of the second row goes to the
// first tile, and Down from it to the left tile below.
int tileMove(int selected, int count, Action a, bool spanFirst = false);

}  // namespace rc
