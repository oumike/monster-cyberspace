#pragma once

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A cell holding this code point is drawn as a thin vertical line (2-3 px)
// down its right side in the cell's fg, or along the panel's right edge when
// it is the row's last cell; never as a glyph. Widgets put it in a column
// they keep blank, so the bar can't cover text.
constexpr char32_t kScrollBarCp = 0x10FFF0;

// Where the thumb sits on a bar `rows` tall showing `visible` of `total`
// items from `first`: at least one row, flush with the ends at the top and
// bottom of the content.
struct ScrollThumb {
  int pos, len;
};
ScrollThumb scrollThumb(int rows, int total, int visible, int first);

// True when `total` items don't fit in `visible`: the widget keeps a column
// for the bar.
inline bool needsScrollBar(int total, int visible) { return visible > 0 && total > visible; }

// Draws the bar in column col over rows [top, bottom): the track in barBg,
// the thumb in fg. Does nothing when everything fits.
void drawScrollBar(Grid& g, int col, int top, int bottom, int total, int visible, int first,
                   const Theme& t, uint16_t bg);

}  // namespace rc
