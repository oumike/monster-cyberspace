#include "ui/widgets/status_bar.h"

#include <algorithm>
#include <string>

#include "util/utf8.h"

namespace rc {

void renderStatusBar(Grid& g, const Theme& t, std::u32string_view title, int count,
                     char32_t connMarker, int wifiBars, int batteryPct, std::string_view clock) {
  const int row = 0;
  g.fillRow(row, t.barBg);

  std::u32string right;
  right += connMarker;
  right += U' ';
  static const char32_t kBars[] = {U'\u2582', U'\u2584', U'\u2586', U'\u2588'};
  int bars = std::clamp(wifiBars, 0, 4);
  for (int i = 0; i < 4; ++i) right += i < bars ? kBars[i] : U'\u00b7';
  if (batteryPct >= 0) right += U" " + utf8ToU32(std::to_string(std::min(batteryPct, 100)) + "%");
  if (!clock.empty()) right += U" " + utf8ToU32(clock);
  right += U' ';

  int rightStart = std::max(0, g.cols() - static_cast<int>(right.size()));
  g.putText(rightStart, row, right, t.barFg, t.barBg, g.cols() - rightStart);

  std::u32string left = U" ";
  left += title;
  if (count >= 0) left += U" " + utf8ToU32(std::to_string(count));
  int leftMax = std::max(0, rightStart - 1);
  if (static_cast<int>(left.size()) > leftMax && leftMax > 0)
    left = left.substr(0, leftMax - 1) + U"\u2026";
  g.putText(0, row, left, t.barFg, t.barBg, leftMax);
}

}  // namespace rc
