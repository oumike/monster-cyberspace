#pragma once

#include <string>
#include <string_view>

#include "hal/board.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/wrap.h"
#include "util/utf8.h"

namespace rc {

inline int putLine(Grid& g, int col, int row, std::string_view utf8, uint16_t fg, uint16_t bg) {
  return g.putText(col, row, utf8ToU32(std::string(utf8)), fg, bg, g.cols() - col);
}

// A portrait grid: rows to spare but not columns, so long text wraps there
// instead of being cut. Landscape grids keep one line per item.
inline bool narrowGrid(const Grid& g) { return g.cols() < 40; }

// utf8 wrapped to the grid width from (col, row), stopping above the hint
// row. Returns the row after the last one drawn.
inline int putWrapped(Grid& g, int col, int row, std::string_view utf8, uint16_t fg,
                      uint16_t bg) {
  for (const std::u32string& line :
       wrapText(utf8ToU32(std::string(utf8)), g.cols() - col, g.cols() - col)) {
    if (row >= g.rows() - 1) break;
    g.putText(col, row++, line, fg, bg, g.cols() - col);
  }
  return row;
}

// The last row in the hint colours.
inline void drawHint(Grid& g, const Theme& t, std::u32string_view text) {
  int row = g.rows() - 1;
  g.fillRow(row, t.hintBg);
  g.putText(0, row, text, t.hintFg, t.hintBg, g.cols());
}

// Keyed boards show `keys`; touch boards show `touch`.
inline void drawHint(Grid& g, const Theme& t, std::u32string_view keys, std::u32string_view touch) {
  drawHint(g, t, board::hasTouch() ? touch : keys);
}

}  // namespace rc
