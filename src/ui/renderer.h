#pragma once

#include <cstdint>
#include <vector>

#include "ui/grid.h"

namespace rc {

struct CellFont;

// Rasterises a cell Grid into a PSRAM frame and pushes only the dirty rows.
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

 private:
  // What was drawn last frame, per grid: the screen's (firstRow 0) and one
  // docked grid.
  struct Layer {
    int firstRow = -1;
    int top = 0;                 // px of row 0
    std::vector<RowSpan> spans;  // where each row was drawn, relative to firstRow
    int gapY = -1;               // where the gap above the last row began
  };
  uint16_t* frame_ = nullptr;
  int w_ = 0, h_ = 0;
  Layer main_{0}, docked_;

  // yoff: px of span above the glyphs, in the row's background.
  void drawRow(const Grid& g, int r, const CellFont& font, RowSpan span, int yoff = 0);
};

}  // namespace rc
