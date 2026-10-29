#include "ui/widgets/scroll_bar.h"

#include <algorithm>

namespace rc {

ScrollThumb scrollThumb(int rows, int total, int visible, int first) {
  if (rows <= 0 || total <= 0 || visible >= total) return ScrollThumb{0, std::max(rows, 0)};
  const int len = std::clamp(rows * visible / total, 1, rows);
  const int maxFirst = total - visible;
  first = std::clamp(first, 0, maxFirst);
  // Rounded, so only the very top and the very bottom touch the ends.
  int pos = (first * (rows - len) + maxFirst / 2) / maxFirst;
  if (first > 0 && pos == 0 && rows - len > 0) pos = 1;
  if (first < maxFirst && pos == rows - len && pos > 0) pos = rows - len - 1;
  return ScrollThumb{pos, len};
}

void drawScrollBar(Grid& g, int col, int top, int bottom, int total, int visible, int first,
                   const Theme& t, uint16_t bg) {
  const int rows = bottom - top;
  if (rows <= 0 || !needsScrollBar(total, visible)) return;
  const ScrollThumb th = scrollThumb(rows, total, visible, first);
  for (int r = 0; r < rows; ++r) {
    const bool on = r >= th.pos && r < th.pos + th.len;
    g.put(col, top + r, kScrollBarCp, on ? t.fg : t.barBg, bg);
  }
}

}  // namespace rc
