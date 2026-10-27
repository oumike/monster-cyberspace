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

// How tall a row is drawn: Normal in the text size's font, Big in the next
// font up (fewer columns fit), Skip not at all.
enum class RowKind : uint8_t { Normal, Big, Skip };

// A screen of character cells with per-row dirty tracking. A row is dirty
// when its cells differ from what they were at the last clearDirty() (the
// last frame drawn), so redrawing a screen from scratch each frame only
// pushes the rows that really changed. Out-of-range puts are ignored.
class Grid {
 public:
  Grid(int cols, int rows);
  int cols() const { return cols_; }
  int rows() const { return rows_; }
  // Clears every cell to bg and every row to RowKind::Normal.
  void fill(uint16_t bg);
  void fillRow(int row, uint16_t bg);
  void put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg);
  // Writes up to maxCols cells of s from col; returns the number written.
  int putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg, int maxCols);
  // Draws a box on the edges of cells [c0..c1] x [r0..r1] (inclusive). Call it
  // after writing the cells: put() clears a cell's edges.
  void outline(int c0, int r0, int c1, int r1, uint16_t color);
  // ORs edges into one cell's edges, in color.
  void addEdges(int col, int row, uint8_t edges, uint16_t color);
  void setRowKind(int row, RowKind k);
  RowKind rowKind(int row) const;
  // Draws the row px tall whatever its kind, for blank padding rows; -1 (the
  // default, and after fill()) goes by the kind.
  void setRowPad(int row, int px);
  int rowPad(int row) const;
  // Draws the row's cells px to the right (0 by default and after fill()),
  // for centring to the pixel; the strip on the left takes cell 0's background.
  void setRowShift(int row, int px);
  int rowShift(int row) const;
  const Cell& at(int col, int row) const;
  bool rowDirty(int row) const;
  void clearDirty();
  // Clears all cells and marks every row dirty.
  void resize(int cols, int rows);

 private:
  int cols_ = 0, rows_ = 0;
  std::vector<Cell> cells_, drawn_;
  std::vector<bool> forced_;  // rows dirty regardless of content (after resize)
  std::vector<RowKind> kinds_;
  std::vector<int16_t> pads_, shifts_;
};

enum class TextSize { Comfortable, Compact };
struct CellSize {
  int w, h;
};
CellSize cellSize(TextSize s);
void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows);
// The font one step up from s, for RowKind::Big rows, and how many of its
// cells fit across the panel.
CellSize bigCellSize(TextSize s);
int bigCols(int panelW, TextSize s);

// Where each row is drawn, in pixels. Rows stack from the top by kind
// (Normal rowH, Big bigH, Skip 0) or by their pad height; the last row stays at (rows-1)*rowH, so
// the hint keeps its place, and a row reaching past it is cut short.
struct RowSpan {
  int y, h;
  int x = 0;  // the row's shift (Grid::setRowShift)
};
std::vector<RowSpan> rowSpans(const Grid& g, int rowH, int bigH);

}  // namespace rc
