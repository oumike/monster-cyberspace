#pragma once

#include <cstdint>
#include <vector>

#include "ui/grid.h"

namespace rc {

struct CellFont;

// Rasterises a cell Grid into a PSRAM frame and pushes only the dirty rows.
// Without PSRAM for a whole frame (the X4's 480x800 on an ESP32-C3) it draws
// each pixel line into a strip one line tall and pushes that; the panel keeps
// the picture. A board with no frame of its own (board::setRedraw true: the
// X4) only learns which rows changed, and pulls every line at refresh time
// through redrawAll.
class Renderer {
 public:
  bool begin();
  // Re-reads the panel size after a rotation; reallocates only if it grew.
  // The caller redraws everything.
  void resize();
  // Draws every dirty row of g with the font for s (RowKind::Big rows with
  // the next font up), placed by rowSpans, then clears g's dirty flags. A row
  // that moved or changed height is redrawn even if its cells didn't change.
  // After a text-size change, the caller marks every row dirty (Grid::fill or
  // resize). firstRow places g's row 0 at that panel row (a docked grid
  // below the screen's, such as the on-screen keyboard), offsetPx lower. A
  // header row 0 (Grid::setHeader) is drawn in the big font, centred.
  void draw(Grid& g, TextSize s, int firstRow = 0, int offsetPx = 0);
  // The frame as last drawn, width() x height() RGB565 (null without memory,
  // or in strip mode).
  const uint16_t* frame() const { return strip_ ? nullptr : frame_; }
  int width() const { return w_; }
  int height() const { return h_; }
  // Pull mode: draws the last main grid drawn, every panel line from the top,
  // a line at a time through board::pushRows. Leaves dirty flags alone. A
  // docked grid isn't drawn (pull-mode boards have no touch keyboard).
  void redrawAll();

 private:
  // What was drawn last frame, per grid: the screen's (firstRow 0) and one
  // docked grid.
  struct Layer {
    int firstRow = -1;
    int top = 0;                 // px of row 0
    std::vector<RowSpan> spans;  // where each row was drawn, relative to firstRow
    int gapY = -1;               // where the gap above the last row began
  };
  uint16_t* frame_ = nullptr;  // the whole frame, or the strip
  int w_ = 0, h_ = 0;
  bool strip_ = false;
  bool pull_ = false;            // the board pulls lines (redrawAll)
  const Grid* pullGrid_ = nullptr;
  TextSize pullSize_ = TextSize::Comfortable;
  int stripRows_ = 0;  // the strip's height
  int base_ = 0;       // the panel row the strip's row 0 is

  std::vector<const uint8_t*> glyphs_;  // drawRow's cell glyphs, kept to reuse the memory

  bool allocate();
  bool ensureStrip(int rows);
  uint16_t* row(int y) { return frame_ + static_cast<size_t>(y - base_) * w_; }
  void push(int y0, int y1);
  Layer main_{0}, docked_;

  // yoff: px of span above the glyphs, in the row's background.
  void drawRow(const Grid& g, int r, const CellFont& font, RowSpan span, int yoff = 0);
  // Row r at span, in the big font when it is RowKind::Big or a header row 0.
  void drawGridRow(const Grid& g, int r, const CellFont& font, const CellFont& big, RowSpan span);
};

}  // namespace rc
