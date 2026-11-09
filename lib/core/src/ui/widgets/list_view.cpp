#include "ui/widgets/list_view.h"

#include "ui/widgets/scroll_bar.h"

#include <algorithm>

#include "ui/tap_map.h"

namespace rc {

void ListView::setItems(std::vector<ListItem> items) {
  items_ = std::move(items);
  setSelected(sel_);
}

void ListView::setSelected(int i) {
  int n = static_cast<int>(items_.size());
  sel_ = n == 0 ? 0 : std::clamp(i, 0, n - 1);
}

bool ListView::select(int i) {
  if (i < 0 || i >= static_cast<int>(items_.size())) return false;
  sel_ = i;
  return true;
}

void ListView::up() {
  const int n = static_cast<int>(items_.size());
  if (!n) return;
  if (sel_ >= cols_) {
    sel_ -= cols_;
    return;
  }
  // Wrap to the last item in this column.
  int i = (n - 1) / cols_ * cols_ + sel_;
  if (i >= n) i -= cols_;
  sel_ = std::max(i, 0);
}

void ListView::down() {
  const int n = static_cast<int>(items_.size());
  if (!n) return;
  if (sel_ + cols_ < n)
    sel_ += cols_;
  else if (sel_ / cols_ < (n - 1) / cols_)
    sel_ = n - 1;  // the row below ends before this column
  else
    sel_ %= cols_;
}

bool ListView::atRightEdge() const {
  return sel_ % cols_ == cols_ - 1 || sel_ + 1 >= static_cast<int>(items_.size());
}

bool ListView::left() {
  if (cols_ == 1 || atLeftEdge()) return false;
  --sel_;
  return true;
}

bool ListView::right() {
  if (cols_ == 1 || atRightEdge()) return false;
  ++sel_;
  return true;
}

void ListView::render(Grid& g, int top, int bottom, int left, int width, const Theme& t) const {
  const int height = bottom - top;
  if (height <= 0 || width <= 0) return;
  const int total = static_cast<int>(items_.size());
  const int rows = (total + cols_ - 1) / cols_;
  const int first = std::max(0, sel_ / cols_ - height + 1);
  // A list longer than the rows keeps its last column for the scroll bar.
  const bool bar = needsScrollBar(rows, height) && width > 3;
  if (bar) --width;
  const int cw = width / cols_;
  if (activeTaps()) activeTaps()->setList(listId_, sel_, total, true);
  for (int r = 0; r < height; ++r) {
    const int row = top + r;
    for (int c = 0; c < width; ++c) g.put(left + c, row, U' ', t.fg, t.bg);
    for (int col = 0; col < cols_; ++col) {
      const int idx = (first + r) * cols_ + col;
      if (idx >= total) break;
      // The last cell takes any spare columns.
      const int x = left + col * cw, w = col == cols_ - 1 ? width - col * cw : cw;
      if (activeTaps()) {
        Target tt;
        tt.kind = TargetKind::Row;
        tt.list = listId_;
        tt.index = static_cast<int16_t>(idx);
        activeTaps()->addCells(x, row, x + w - 1, row, tt);
      }
      const ListItem& it = items_[idx];
      const uint16_t fg = it.tone == ItemTone::Error ? t.error : t.fg, bg = t.bg;
      const int rw = static_cast<int>(it.right.size());
      const int labelMax = std::max(0, w - 2 - (rw ? rw + 2 : 0));
      if (it.marker) g.put(x, row, U'\u2022', fg, bg);
      g.putText(x + 2, row, it.label, fg, bg, labelMax);
      if (rw && rw + 1 <= w) g.putText(x + w - 1 - rw, row, it.right, fg, bg, rw);
      if (idx == sel_ && cursorShown()) g.outline(x, row, x + w - 1, row, t.selectBg);
    }
  }
  if (bar) drawScrollBar(g, left + width, top, bottom, rows, height, first, t, t.bg);
}

}  // namespace rc
