#include "ui/button_grid.h"

#include <algorithm>

namespace rc {

ButtonGrid buttonGrid(int cols, int top, int bottom, int count, int columns, int scrollRow) {
  ButtonGrid out;
  columns = std::max(1, columns);
  const int width = (cols - 1) / columns - 1;  // a blank column before each, and after the last
  const int pitch = kButtonRows + 1;
  const int visibleRows = std::max(0, (bottom - top + 1) / pitch);
  if (width < 3 || visibleRows == 0 || count <= 0) return out;
  const int totalRows = (count + columns - 1) / columns;
  out.maxScroll = std::max(0, totalRows - visibleRows);
  out.visibleRows = visibleRows;
  out.totalRows = totalRows;
  out.scrollRow = std::min(std::max(0, scrollRow), out.maxScroll);
  out.first = out.scrollRow * columns;
  // Centre the columns: the spare columns split before and after.
  const int left = 1 + (cols - 1 - columns * (width + 1)) / 2;
  for (int i = out.first; i < count; ++i) {
    const int row = i / columns - out.scrollRow;
    if (row >= visibleRows) break;
    const int col = i % columns;
    const int c0 = left + col * (width + 1);
    const int r0 = top + row * pitch;
    out.rects.push_back(TileRect{c0, r0, c0 + width - 1, r0 + kButtonRows - 1});
  }
  return out;
}

}  // namespace rc
