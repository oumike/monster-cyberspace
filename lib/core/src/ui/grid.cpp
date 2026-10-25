#include "ui/grid.h"

#include <algorithm>

namespace rc {

Grid::Grid(int cols, int rows) { resize(cols, rows); }

void Grid::resize(int cols, int rows) {
  cols_ = std::max(cols, 0);
  rows_ = std::max(rows, 0);
  cells_.assign(static_cast<size_t>(cols_) * rows_, Cell{});
  drawn_ = cells_;
  forced_.assign(rows_, true);
}

void Grid::fill(uint16_t bg) {
  for (int r = 0; r < rows_; ++r) fillRow(r, bg);
}

void Grid::fillRow(int row, uint16_t bg) {
  for (int c = 0; c < cols_; ++c) put(c, row, U' ', 0, bg);
}

void Grid::put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg) {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
  cells_[static_cast<size_t>(row) * cols_ + col] = Cell{cp, fg, bg};
}

int Grid::putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg,
                  int maxCols) {
  int n = 0;
  for (char32_t cp : s) {
    if (n >= maxCols || col + n >= cols_) break;
    put(col + n, row, cp, fg, bg);
    ++n;
  }
  return n;
}

const Cell& Grid::at(int col, int row) const {
  static const Cell blank{};
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return blank;
  return cells_[static_cast<size_t>(row) * cols_ + col];
}

bool Grid::rowDirty(int row) const {
  if (row < 0 || row >= rows_) return false;
  if (forced_[row]) return true;
  auto first = cells_.begin() + static_cast<size_t>(row) * cols_;
  return !std::equal(first, first + cols_, drawn_.begin() + static_cast<size_t>(row) * cols_);
}

void Grid::clearDirty() {
  drawn_ = cells_;
  std::fill(forced_.begin(), forced_.end(), false);
}

CellSize cellSize(TextSize s) {
  return s == TextSize::Compact ? CellSize{6, 12} : CellSize{8, 16};
}

void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows) {
  CellSize c = cellSize(s);
  cols = panelW / c.w;
  rows = panelH / c.h;
}

}  // namespace rc
