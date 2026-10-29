#include "ui/widgets/status_bar.h"

#include <algorithm>
#include <string>

#include "ui/tap_map.h"
#include "util/utf8.h"

namespace rc {

void renderStatusBar(Grid& g, const Theme& t, std::u32string_view title, int count,
                     char32_t connMarker, int wifiBars, int batteryPct, std::string_view clock,
                     StatusTouch touch) {
  const int row = 0;
  g.fillRow(row, t.barBg);
  // A header row (Grid::setHeader) is drawn in a wider font: fewer cells fit.
  const HeaderSize& hd = g.header();
  const int cols = hd.cols > 0 ? std::min(hd.cols, g.cols()) : g.cols();

  std::u32string right;
  right += connMarker;
  right += U' ';
  static const char32_t kBars[] = {U'\u2582', U'\u2584', U'\u2586', U'\u2588'};
  int bars = std::clamp(wifiBars, 0, 4);
  for (int i = 0; i < 4; ++i) right += i < bars ? kBars[i] : U'\u00b7';
  if (batteryPct >= 0) right += U" " + utf8ToU32(std::to_string(std::min(batteryPct, 100)) + "%");
  if (!clock.empty()) right += U" " + utf8ToU32(clock);
  if (touch.on && touch.menu) right += U" \u22ef";  // ⋯, its own cell after the clock
  right += U' ';

  int rightStart = std::max(0, cols - static_cast<int>(right.size()));
  g.putText(rightStart, row, right, t.barFg, t.barBg, cols - rightStart);

  int titleCol = 0;
  std::u32string left;
  if (touch.on && touch.back) {
    g.put(0, row, U'\u2039', t.barFg, t.barBg);  // ‹
    titleCol = 2;
    left = std::u32string(title);
  } else {
    left = U" ";
    left += title;
  }
  if (count >= 0) left += U" " + utf8ToU32(std::to_string(count));
  int leftMax = std::max(0, rightStart - 1 - titleCol);
  if (static_cast<int>(left.size()) > leftMax && leftMax > 0)
    left = left.substr(0, leftMax - 1) + U"\u2026";
  g.putText(titleCol, row, left, t.barFg, t.barBg, leftMax);
  // An outlined theme's bar is the page colour: a rule sets it apart.
  if (t.outlined)
    for (int c = 0; c < g.cols(); ++c) g.addEdges(c, row, kEdgeBottom, t.barFg);

  if (touch.on && activeTaps()) {
    if (touch.back) {
      Target b;
      b.kind = TargetKind::Fixed;
      b.tap.action = Action::Back;
      b.hold.action = Action::OpenHome;
      activeTaps()->addCells(0, row, 3, row, b, hd.cellW);
    }
    if (touch.menu) {
      Target m;
      m.kind = TargetKind::Fixed;
      m.tap.action = Action::Palette;
      activeTaps()->addCells(cols - 4, row, cols - 1, row, m, hd.cellW);
    }
  }
}

}  // namespace rc
