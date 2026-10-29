#include "ui/widgets/side_nav.h"

#include <algorithm>

#include "ui/tap_map.h"

namespace rc {

int sideNavCols(int cellW) {
  if (cellW <= 0) return 3;
  return std::max(3, (36 + cellW - 1) / cellW);
}

void renderSideNav(Grid& g, int top, int bottom, int cols, const Theme& t) {
  const int h = bottom - top;
  if (h < 3 || cols < 3 || cols > g.cols()) return;
  const int c0 = g.cols() - cols, c1 = g.cols() - 1;
  // The middle button takes what the even split leaves over.
  const int edge = h / 3;
  const int r0[3] = {top, top + edge, bottom - edge};
  const int r1[3] = {top + edge - 1, bottom - edge - 1, bottom - 1};
  const char32_t glyph[3] = {U'\u25B2', U'\u25CF', U'\u25BC'};
  const Action act[3] = {Action::Up, Action::Select, Action::Down};
  for (int r = top; r < bottom; ++r)
    for (int c = c0; c <= c1; ++c) g.put(c, r, U' ', t.fg, t.bg);
  for (int i = 0; i < 3; ++i) {
    g.put(c0 + (cols - 1) / 2, (r0[i] + r1[i]) / 2, glyph[i], t.fg, t.bg);
    g.outline(c0, r0[i], c1, r1[i], t.dim);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Fixed;
      tt.tap.action = act[i];
      activeTaps()->addCells(c0, r0[i], c1, r1[i], tt);
    }
  }
}

}  // namespace rc
