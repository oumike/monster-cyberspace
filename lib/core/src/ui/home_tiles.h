#pragma once

#include <cstdint>
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

// The fallback when homeLayout fits no grid: one column of tiles 3 rows tall,
// as many as fit (at most count), each `percent` of the panel's width, and
// the screen scrolls through the rest. Pads and centring as in homeLayout;
// with gaps false the tiles touch. tiles holds the visible slots, top first,
// and is empty when not even one fits or a tile would be under 10 cols wide.
HomeLayout homeColumn(int panelW, int areaPx, int gridRows, CellSize cell, int count,
                      int percent, bool gaps = true);

// A rectangle in px.
struct PxRect {
  int x, y, w, h;
};

// Home's buttons on a touch board: `count` one-line buttons in two columns,
// row-major, stacked from the top of an areaPx-tall area with kHomeGapPx
// above each row. Every button is the same size: the longest label
// (labelCells cells) plus kHomePadPx and a 1 px outline on each side, and a
// cell high plus padY (kHomePadPx, or double it where there is room) and the
// outline above and below. The left column's buttons touch the panel's left
// edge, the right column's its right edge. rects is empty when the columns
// would come closer than kHomeGapPx or the rows don't fit areaPx.
// spanFirst: the first button takes the top row alone, as wide as the
// panel, and the rest pair up in the columns under it (odd items right).
// side: each button's column, -1 left, 0 across, 1 right.
constexpr int kHomeGapPx = 6;
constexpr int kHomePadPx = 4;
struct HomeButtons {
  std::vector<PxRect> rects;
  std::vector<int8_t> side;
};
HomeButtons homeButtons(int panelW, int areaPx, CellSize cell, int count, int labelCells,
                        int padY = kHomePadPx, bool spanFirst = false);
// With nothing between the columns: widens the left and right buttons to
// meet kHomeGapPx apart in the middle of a panelW-wide panel, the left still
// against the left edge and the right against the right. Spanning buttons
// and the rows are left alone.
void widenHomeButtons(HomeButtons& b, int panelW);
// The arrow keys between home buttons by where they lie: Up/Down to the
// nearest button above/below, Left/Right to the nearest in the same row.
int buttonMove(const std::vector<PxRect>& rects, int selected, Action a);

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
