#include "ui/widgets/list_view.h"

#include <algorithm>

namespace rc {

void ListView::setItems(std::vector<ListItem> items) {
  items_ = std::move(items);
  setSelected(sel_);
}

void ListView::setSelected(int i) {
  int n = static_cast<int>(items_.size());
  sel_ = n == 0 ? 0 : std::clamp(i, 0, n - 1);
}

void ListView::up() {
  int n = static_cast<int>(items_.size());
  if (n) sel_ = (sel_ + n - 1) % n;
}

void ListView::down() {
  int n = static_cast<int>(items_.size());
  if (n) sel_ = (sel_ + 1) % n;
}

void ListView::render(Grid& g, int top, int bottom, int left, int width, const Theme& t) const {
  const int height = bottom - top;
  if (height <= 0 || width <= 0) return;
  int first = std::max(0, sel_ - height + 1);
  for (int r = 0; r < height; ++r) {
    int row = top + r;
    int idx = first + r;
    bool sel = idx == sel_ && idx < static_cast<int>(items_.size());
    for (int c = 0; c < width; ++c) g.put(left + c, row, U' ', t.fg, t.bg);
    if (idx >= static_cast<int>(items_.size())) continue;
    const ListItem& it = items_[idx];
    uint16_t fg = it.fg, bg = t.bg;
    int rw = static_cast<int>(it.right.size());
    int labelMax = std::max(0, width - 2 - (rw ? rw + 2 : 0));
    if (it.marker) g.put(left, row, U'\u2022', fg, bg);
    g.putText(left + 2, row, it.label, fg, bg, labelMax);
    if (rw && rw + 1 <= width) g.putText(left + width - 1 - rw, row, it.right, fg, bg, rw);
    if (sel) g.outline(left, row, left + width - 1, row, t.selectBg);
  }
}

}  // namespace rc
