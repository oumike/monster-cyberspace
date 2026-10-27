#include "ui/home_tiles.h"

#include <algorithm>

namespace rc {

HomeLayout homeLayout(int panelW, int areaPx, int gridRows, CellSize cell, int count,
                      int percent) {
  HomeLayout out;
  if (count <= 0 || cell.w <= 0 || cell.h <= 0 || areaPx <= 0) return out;
  // Across: an odd number of cells, so the gap column leaves equal halves.
  TileSpan across = tileSpan(panelW, cell.w, percent);
  if (across.cells % 2 == 0) --across.cells;
  const int tileW = (across.cells - 1) / 2;
  if (tileW < 10) return out;
  // Down: numRows tiles of h rows with gap px between them.
  const int numRows = (count + 1) / 2;
  const int gap = cell.w;
  const int avail = areaPx * percent / 100 - (numRows - 1) * gap;
  int h = avail > 0 ? (avail / numRows + cell.h / 2) / cell.h : 0;
  auto totalPx = [&](int rows) { return numRows * rows * cell.h + (numRows - 1) * gap; };
  auto lastRow = [&](int rows) { return 1 + numRows * rows + (numRows - 1); };
  while (h >= 3 && (totalPx(h) > areaPx || lastRow(h) > gridRows - 2)) --h;
  if (h < 3) return out;

  out.shiftPx = (panelW - across.cells * cell.w) / 2;
  out.lastRow = lastRow(h);
  out.pads.emplace_back(1, (areaPx - totalPx(h)) / 2);
  out.tiles.reserve(count);
  for (int i = 0; i < numRows; ++i) {
    const int r0 = 2 + i * (h + 1);
    if (i + 1 < numRows) out.pads.emplace_back(r0 + h, gap);
    for (int j = 0; j < 2 && i * 2 + j < count; ++j) {
      const int c0 = j * (tileW + 1);
      out.tiles.push_back(TileRect{c0, r0, c0 + tileW - 1, r0 + h - 1});
    }
  }
  return out;
}

TileSpan tileSpan(int areaPx, int unitPx, int percent) {
  if (areaPx <= 0 || unitPx <= 0) return TileSpan{0, 0};
  int cells = (areaPx * percent / 100 + unitPx / 2) / unitPx;
  cells = std::min(cells, areaPx / unitPx);
  return TileSpan{cells, (areaPx - cells * unitPx) / 2};
}

int tileMove(int selected, int count, Action a) {
  if (count <= 0) return selected;
  const int col = selected % 2;
  const int row = selected / 2;
  switch (a) {
    case Action::Up:
      if (row > 0) selected -= 2;
      break;
    case Action::Down:
      if (selected + 2 < count) selected += 2;
      break;
    case Action::Left:
      if (col > 0) selected -= 1;
      break;
    case Action::Right:
      if (col == 0 && selected + 1 < count) selected += 1;
      break;
    default:
      break;
  }
  return selected;
}

}  // namespace rc
