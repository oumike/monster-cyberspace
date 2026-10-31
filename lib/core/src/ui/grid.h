#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/eink.h"

namespace rc {

// Cell edges drawn as 1 px lines, for outlining a selection.
constexpr uint8_t kEdgeTop = 1, kEdgeBottom = 2, kEdgeLeft = 4, kEdgeRight = 8;

#ifndef RC_LOW_MEM
#define RC_LOW_MEM 0
#endif

#if RC_LOW_MEM
// A board without PSRAM (the X4, black and white e-paper): 4 bytes a cell,
// each colour kept only as light (1) or dark (0). Read colours through
// cellColor().
struct Cell {
  Cell(char32_t c = U' ', uint16_t f = 0, uint16_t b = 0)
      : cp(c), fg(light(f)), bg(light(b)), edgeColor(0), edges(0), bold(false), nudge(false) {}
  static uint32_t light(uint16_t c) { return rgb565Dark(c) ? 0 : 1; }
  void setEdgeColor(uint16_t c) { edgeColor = light(c); }
  uint32_t cp : 21;
  uint32_t fg : 1, bg : 1, edgeColor : 1;
  uint32_t edges : 4;
  uint32_t bold : 1;
  uint32_t nudge : 1;  // the glyph is drawn half a cell right (Grid::nudge)
  bool operator==(const Cell& o) const {
    return cp == o.cp && fg == o.fg && bg == o.bg && edges == o.edges && bold == o.bold &&
           nudge == o.nudge && (!edges || edgeColor == o.edgeColor);
  }
  bool operator!=(const Cell& o) const { return !(*this == o); }
};
static_assert(sizeof(Cell) == 4, "Cell grew; the X4 grid no longer fits");
inline uint16_t cellColor(uint32_t c) { return c ? 0xFFFF : 0x0000; }
#else
// 12 bytes.
struct Cell {
  Cell(char32_t c = U' ', uint16_t f = 0, uint16_t b = 0)
      : cp(c), fg(f), bg(b), edgeColor(0), edges(0), bold(false), nudge(false) {}
  void setEdgeColor(uint16_t c) { edgeColor = c; }
  char32_t cp;
  uint16_t fg, bg;
  uint16_t edgeColor;
  uint8_t edges : 4;
  bool bold : 1;
  bool nudge : 1;  // the glyph is drawn half a cell right (Grid::nudge)
  bool operator==(const Cell& o) const {
    return cp == o.cp && fg == o.fg && bg == o.bg && edges == o.edges && bold == o.bold &&
           nudge == o.nudge && (!edges || edgeColor == o.edgeColor);
  }
  bool operator!=(const Cell& o) const { return !(*this == o); }
};
static_assert(sizeof(Cell) == 12, "Cell grew");
inline uint16_t cellColor(uint16_t c) { return c; }
#endif

// How tall a row is drawn: Normal in the text size's font, Big in the next
// font up (fewer columns fit), Skip not at all.
enum class RowKind : uint8_t { Normal, Big, Skip };

// A taller row 0 for the status bar: px tall, drawn in the RowKind::Big font,
// cols cells of cellW px across. px 0 leaves row 0 a normal row.
struct HeaderSize {
  int px = 0, cols = 0, cellW = 0;
};

// A screen of character cells with per-row dirty tracking. A row is dirty
// when its cells differ (by hash) from what they were at the last clearDirty() (the
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
  // Writes up to maxCols cells of s from col; returns the number written. A
  // tray emoji takes two cells (its tail code point fills the second), and is
  // left out whole when it does not fit.
  int putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg, int maxCols);
  // Draws a box on the edges of cells [c0..c1] x [r0..r1] (inclusive). Call it
  // after writing the cells: put() clears a cell's edges.
  void outline(int c0, int r0, int c1, int r1, uint16_t color);
  // Draws cells [c0..c1] of a row bold. Call it after writing the cells: put()
  // clears it.
  void embolden(int c0, int c1, int row);
  // Draws the glyphs of cells [c0..c1] of a row half a cell to the right, into
  // the cell after c1, for centring text to the pixel. Call it after writing
  // the cells: put() clears it.
  void nudge(int c0, int c1, int row);
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
  // Row 0 as a header (HeaderSize). Kept across fill() and resize().
  void setHeader(HeaderSize h) { header_ = h; }
  const HeaderSize& header() const { return header_; }
  // Extra px drawn under the last row in its background, so a grid ending
  // above the panel's bottom edge leaves no stale strip. Kept across fill()
  // and resize().
  void setBottomPad(int px) { bottomPad_ = px < 0 ? 0 : px; }
  int bottomPad() const { return bottomPad_; }
  // Stacked: every row, the last too, goes straight under the one before at
  // its own height (pad or kind), and the bottom pad goes under the last; no
  // header. Kept across fill() and resize().
  void setStacked(bool on) { stacked_ = on; }
  bool stacked() const { return stacked_; }
  const Cell& at(int col, int row) const;
  // Copies one cell (glyph, colours, marks) to another; out of range is ignored.
  void copyCell(int fromCol, int fromRow, int toCol, int toRow);
  bool rowDirty(int row) const;
  void clearDirty();
  // Clears all cells and marks every row dirty.
  void resize(int cols, int rows);

 private:
  int cols_ = 0, rows_ = 0;
  uint64_t rowHash(int row) const;
  std::vector<Cell> cells_;
  // Each row's hash at the last clearDirty(): a copy of the cells would double
  // the grid, too much for a board without PSRAM.
  std::vector<uint64_t> drawn_;
  std::vector<bool> forced_;  // rows dirty regardless of content (after resize)
  std::vector<RowKind> kinds_;
  std::vector<int16_t> pads_, shifts_;
  HeaderSize header_;
  int bottomPad_ = 0;
  bool stacked_ = false;
};

// The values are saved on the device and sent by the web page: never reorder.
enum class TextSize { Comfortable, Compact, Large };
// Compact -> Comfortable -> Large -> Compact, for the "Text size" menu item.
TextSize nextTextSize(TextSize s);
// "compact", "comfortable", "large": backups and the web page.
const char* textSizeKey(TextSize s);
// false (out unchanged) for anything else.
bool parseTextSize(std::string_view key, TextSize& out);
std::u32string textSizeLabel(TextSize s);  // U"Compact" ...
// The saved index (the enum's value); out of range is Compact.
TextSize textSizeFromIndex(int i);
struct CellSize {
  int w, h;
};
CellSize cellSize(TextSize s);
// headerPx > 0: row 0 is that tall (Grid::setHeader) and the rest cellSize(s).
void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows, int headerPx = 0);
// The font one step up from s, for RowKind::Big rows, and how many of its
// cells fit across the panel.
CellSize bigCellSize(TextSize s);
int bigCols(int panelW, TextSize s);
// The status bar of keyboardless touch boards: 1.5x a row, in the big font,
// so it's easier to tap.
HeaderSize touchHeader(int panelW, TextSize s);

// Where each row is drawn, in pixels. Rows stack from the top by kind
// (Normal rowH, Big bigH, Skip 0) or by their pad height; the last row stays at (rows-1)*rowH, so
// the hint keeps its place, and a row reaching past it is cut short. A header
// (Grid::setHeader) makes row 0 its px tall and moves the last row down by the
// difference; the last row also takes the grid's bottom pad. A stacked grid
// (Grid::setStacked) just stacks every row.
struct RowSpan {
  int y, h;
  int x = 0;  // the row's shift (Grid::setRowShift)
};
std::vector<RowSpan> rowSpans(const Grid& g, int rowH, int bigH);

}  // namespace rc
