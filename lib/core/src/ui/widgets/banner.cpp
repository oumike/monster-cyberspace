#include "ui/widgets/banner.h"

#include "ui/tap_map.h"

namespace rc {

void Banner::show(std::u32string text, int64_t nowMs, int64_t durationMs, bool error) {
  text_ = std::move(text);
  until_ = nowMs + durationMs;
  error_ = error;
}

void Banner::render(Grid& g, int row, const Theme& t, int64_t nowMs) const {
  if (!visible(nowMs)) return;
  if (activeTaps()) {
    Target tt;
    tt.kind = TargetKind::Banner;
    activeTaps()->addCells(0, row, g.cols() - 1, row, tt);
  }
  // Outlined themes box an error rather than filling it.
  const bool fill = error_ && !t.outlined;
  uint16_t bg = fill ? t.error : t.hintBg;
  uint16_t fg = fill ? t.bg : error_ ? t.error : t.hintFg;
  g.fillRow(row, bg);
  g.put(0, row, U' ', fg, bg);
  g.putText(1, row, text_, fg, bg, g.cols() - 1);
  if (error_ && t.outlined) g.outline(0, row, g.cols() - 1, row, t.error);
}

}  // namespace rc
