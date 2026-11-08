#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "lvui/views/view_util.h"

namespace lvui {

// Free text at grid cells, as screens put it with putLine / putWrapped: each
// line a one-cell-high label at (col, row) of its parent's cell grid, cut to
// whole cells (as rc::Grid::putText cuts). Between begin() and end() a screen
// puts what it shows this frame; labels are reused in order, so an unchanged
// frame redraws nothing, and end() hides the ones left over.
class CellText {
 public:
  CellText() = default;
  CellText(const CellText&) = delete;
  CellText& operator=(const CellText&) = delete;

  // Fills the parent; takes no taps.
  void create(lv_obj_t* parent);

  void begin() { used_ = 0; }
  // s at (col, row), at most maxCols cells (-1: to the right edge). Returns
  // the cells it takes, as rc::Grid::putText.
  int line(int col, int row, std::u32string_view s, uint16_t fg, int maxCols = -1);
  int line(int col, int row, std::string_view utf8, uint16_t fg, int maxCols = -1);
  // One line on its own background (a cursor cell).
  int filled(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg);
  // utf8 wrapped (rc::wrapText) to the width from (col, row), stopping above
  // row `stop` (-1: the last row, the hint). Returns the row after the last
  // one put.
  int wrapped(int col, int row, std::string_view utf8, uint16_t fg, int stop = -1);
  void end();

  // The parent's grid in whole cells.
  int cols() const;
  int rows() const;
  Cell cell() const { return cellOf(box_); }

  lv_obj_t* obj() const { return box_; }
  int count() const { return used_; }
  lv_obj_t* label(int i) const { return labels_[i].label; }

 private:
  struct Line {
    lv_obj_t* label;
    bool opaque;
  };
  Line& take();

  lv_obj_t* box_ = nullptr;
  std::vector<Line> labels_;
  int used_ = 0;
};

// The hint row: the last whole row of its parent, in hint colours.
class HintBar {
 public:
  HintBar() = default;
  HintBar(const HintBar&) = delete;
  HintBar& operator=(const HintBar&) = delete;

  void create(lv_obj_t* parent);
  void sync(std::u32string_view text);

  lv_obj_t* obj() const { return row_; }
  lv_obj_t* label() const { return label_; }

 private:
  lv_obj_t *row_ = nullptr, *label_ = nullptr;
};

}  // namespace lvui
