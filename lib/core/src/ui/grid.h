#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rc {

struct Cell {
  char32_t cp = U' ';
  uint16_t fg = 0, bg = 0;
  bool operator==(const Cell& o) const { return cp == o.cp && fg == o.fg && bg == o.bg; }
  bool operator!=(const Cell& o) const { return !(*this == o); }
};

// A screen of character cells with per-row dirty tracking. A put marks its
// row dirty only when the cell actually changes; out-of-range puts are ignored.
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
  const Cell& at(int col, int row) const;
  bool rowDirty(int row) const;
  void clearDirty();
  // Clears all cells and marks every row dirty.
  void resize(int cols, int rows);

 private:
  int cols_ = 0, rows_ = 0;
  std::vector<Cell> cells_;
  std::vector<bool> dirty_;
};

enum class TextSize { Comfortable, Compact };
struct CellSize {
  int w, h;
};
CellSize cellSize(TextSize s);
void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows);

}  // namespace rc
