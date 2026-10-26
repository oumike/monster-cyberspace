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
  kinds_.assign(rows_, RowKind::Normal);
}

void Grid::fill(uint16_t bg) {
  for (int r = 0; r < rows_; ++r) fillRow(r, bg);
  std::fill(kinds_.begin(), kinds_.end(), RowKind::Normal);
}

void Grid::setRowKind(int row, RowKind k) {
  if (row >= 0 && row < rows_) kinds_[row] = k;
}

RowKind Grid::rowKind(int row) const {
  return row >= 0 && row < rows_ ? kinds_[row] : RowKind::Normal;
}

void Grid::addEdges(int col, int row, uint8_t edges, uint16_t color) {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
  Cell& cell = cells_[static_cast<size_t>(row) * cols_ + col];
  cell.edges |= edges;
  cell.edgeColor = color;
}

void Grid::fillRow(int row, uint16_t bg) {
  for (int c = 0; c < cols_; ++c) put(c, row, U' ', 0, bg);
}

void Grid::put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg) {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
  cells_[static_cast<size_t>(row) * cols_ + col] = Cell{cp, fg, bg};
}

void Grid::outline(int c0, int r0, int c1, int r1, uint16_t color) {
  c0 = std::max(c0, 0);
  r0 = std::max(r0, 0);
  c1 = std::min(c1, cols_ - 1);
  r1 = std::min(r1, rows_ - 1);
  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) {
      uint8_t e = (r == r0 ? kEdgeTop : 0) | (r == r1 ? kEdgeBottom : 0) |
                  (c == c0 ? kEdgeLeft : 0) | (c == c1 ? kEdgeRight : 0);
      if (!e) continue;
      Cell& cell = cells_[static_cast<size_t>(r) * cols_ + c];
      cell.edges |= e;
      cell.edgeColor = color;
    }
  }
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

CellSize bigCellSize(TextSize s) {
  return s == TextSize::Compact ? CellSize{8, 14} : CellSize{10, 18};
}

int bigCols(int panelW, TextSize s) { return panelW / bigCellSize(s).w; }

std::vector<RowSpan> rowSpans(const Grid& g, int rowH, int bigH) {
  const int n = g.rows();
  std::vector<RowSpan> out(n, RowSpan{0, 0});
  if (n == 0) return out;
  const int lastY = (n - 1) * rowH;
  int y = 0;
  for (int r = 0; r < n - 1; ++r) {
    RowKind k = g.rowKind(r);
    int h = k == RowKind::Skip ? 0 : k == RowKind::Big ? bigH : rowH;
    h = std::max(0, std::min(h, lastY - y));
    out[r] = RowSpan{y, h};
    y += h;
  }
  out[n - 1] = RowSpan{lastY, rowH};
  return out;
}

}  // namespace rc
