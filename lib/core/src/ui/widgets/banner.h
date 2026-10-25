#pragma once

#include <cstdint>
#include <string>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A one-row message that disappears after a duration.
class Banner {
 public:
  void show(std::u32string text, int64_t nowMs, int64_t durationMs, bool error);
  void hide() { until_ = 0; }
  bool visible(int64_t nowMs) const { return nowMs < until_; }
  // Draws nothing when not visible.
  void render(Grid& g, int row, const Theme& t, int64_t nowMs) const;

 private:
  std::u32string text_;
  int64_t until_ = 0;
  bool error_ = false;
};

}  // namespace rc
