#include "ui/home_tiles.h"

namespace rc {

std::vector<TileRect> homeTiles(int cols, int rows, int count) {
  std::vector<TileRect> out;
  if (count <= 0 || cols <= 0) return out;
  const int numCols = 2;
  const int numRows = (count + numCols - 1) / numCols;
  const int firstRow = 1;
  const int availH = rows - 2;  // rows 1..rows-2
  if (availH <= 0) return out;
  // Every tile must be at least 3 rows tall and 10 cols wide.
  if (availH / numRows < 3) return out;
  if (cols / numCols < 10) return out;
  out.reserve(count);
  for (int i = 0; i < numRows; ++i) {
    const int r0 = firstRow + availH * i / numRows;
    const int r1 = firstRow + availH * (i + 1) / numRows - 1;
    for (int j = 0; j < numCols; ++j) {
      const int idx = i * numCols + j;
      if (idx >= count) break;
      const int c0 = cols * j / numCols;
      const int c1 = cols * (j + 1) / numCols - 1;
      out.push_back(TileRect{c0, r0, c1, r1});
    }
  }
  return out;
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
