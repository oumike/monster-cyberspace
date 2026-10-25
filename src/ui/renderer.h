#pragma once

#include <cstdint>

#include "ui/grid.h"

namespace rc {

// Rasterises a cell Grid into a PSRAM frame and pushes only the dirty rows.
class Renderer {
 public:
  bool begin();
  // Draws every dirty row of g with the font for s, then clears g's dirty
  // flags. After a text-size change, the caller marks every row dirty
  // (Grid::fill or resize).
  void draw(Grid& g, TextSize s);

 private:
  uint16_t* frame_ = nullptr;
  int w_ = 0, h_ = 0;
};

}  // namespace rc
