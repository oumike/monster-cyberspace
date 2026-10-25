#include "ui/widgets/banner.h"

namespace rc {

void Banner::show(std::u32string text, int64_t nowMs, int64_t durationMs, bool error) {
  text_ = std::move(text);
  until_ = nowMs + durationMs;
  error_ = error;
}

void Banner::render(Grid& g, int row, const Theme& t, int64_t nowMs) const {
  if (!visible(nowMs)) return;
  uint16_t bg = error_ ? t.error : t.hintBg;
  uint16_t fg = error_ ? t.bg : t.hintFg;
  g.fillRow(row, bg);
  g.put(0, row, U' ', fg, bg);
  g.putText(1, row, text_, fg, bg, g.cols() - 1);
}

}  // namespace rc
