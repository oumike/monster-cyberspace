#pragma once

#include <string>
#include <string_view>

#include "hal/board.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "util/utf8.h"

namespace rc {

inline int putLine(Grid& g, int col, int row, std::string_view utf8, uint16_t fg, uint16_t bg) {
  return g.putText(col, row, utf8ToU32(std::string(utf8)), fg, bg, g.cols() - col);
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
