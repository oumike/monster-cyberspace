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
  // Draws every dirty row of g with the font for s (RowKind::Big rows with
  // the next font up), placed by rowSpans, then clears g's dirty flags. A row
  // that moved or changed height is redrawn even if its cells didn't change.
  // After a text-size change, the caller marks every row dirty (Grid::fill or
  // resize).
  void draw(Grid& g, TextSize s);

 private:
  uint16_t* frame_ = nullptr;
  int w_ = 0, h_ = 0;
  std::vector<RowSpan> spans_;  // where each row was drawn last frame
  int gapY_ = -1;               // where the gap above the last row began

  void drawRow(const Grid& g, int r, const CellFont& font, RowSpan span);
};

}  // namespace rc
