#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rc {

// Cell edges drawn as 1 px lines, for outlining a selection.
constexpr uint8_t kEdgeTop = 1, kEdgeBottom = 2, kEdgeLeft = 4, kEdgeRight = 8;

struct Cell {
  char32_t cp = U' ';
  uint16_t fg = 0, bg = 0;
  uint8_t edges = 0;
  uint16_t edgeColor = 0;
  bool operator==(const Cell& o) const {
    return cp == o.cp && fg == o.fg && bg == o.bg && edges == o.edges &&
           (!edges || edgeColor == o.edgeColor);
  }
  bool operator!=(const Cell& o) const { return !(*this == o); }
};

// A screen of character cells with per-row dirty tracking. A row is dirty
// when its cells differ from what they were at the last clearDirty() (the
// last frame drawn), so redrawing a screen from scratch each frame only
// pushes the rows that really changed. Out-of-range puts are ignored.
class Grid {
 public:
  Grid(int cols, int rows);
  int cols() const { return cols_; }
  int rows() const { return rows_; }
  void fill(uint16_t bg);
  void fillRow(int row, uint16_t bg);
  void put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg);
  // Writes up to maxCols cells of s from col; returns the number written.
  int putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg, int maxCols);
  // Draws a box on the edges of cells [c0..c1] x [r0..r1] (inclusive). Call it
  // after writing the cells: put() clears a cell's edges.
  void outline(int c0, int r0, int c1, int r1, uint16_t color);
  const Cell& at(int col, int row) const;
  bool rowDirty(int row) const;
  void clearDirty();
  // Clears all cells and marks every row dirty.
  void resize(int cols, int rows);

 private:
  int cols_ = 0, rows_ = 0;
  std::vector<Cell> cells_, drawn_;
  std::vector<bool> forced_;  // rows dirty regardless of content (after resize)
};

enum class TextSize { Comfortable, Compact };
struct CellSize {
  int w, h;
};
CellSize cellSize(TextSize s);
void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows);

}  // namespace rc
