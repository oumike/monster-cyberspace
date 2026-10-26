#include "ui/widgets/entry_list.h"

#include <algorithm>

namespace rc {

void EntryList::setRows(std::vector<FeedRow> rows) {
  rows_ = std::move(rows);
  setSelected(sel_);
}

void EntryList::setSelected(int i) {
  int n = static_cast<int>(rows_.size());
  sel_ = n == 0 ? 0 : std::clamp(i, 0, n - 1);
}

void EntryList::up() {
  int n = static_cast<int>(rows_.size());
  if (n) sel_ = (sel_ + n - 1) % n;
}

void EntryList::down() {
  int n = static_cast<int>(rows_.size());
  if (n) sel_ = (sel_ + 1) % n;
}

bool EntryList::atEnd() const { return !rows_.empty() && sel_ == static_cast<int>(rows_.size()) - 1; }

void EntryList::setBigHeads(int rowH, int bigH, int headCols) {
  rowH_ = rowH;
  bigH_ = bigH;
  headCols_ = headCols;
}

void EntryList::render(Grid& g, int top, int bottom, const Theme& t) const {
  const int width = g.cols();
  const int height = bottom - top;
  if (height <= 0 || width <= 0) return;
  for (int r = top; r < bottom; ++r) g.fillRow(r, t.bg);
  const bool big = bigH_ > 0 && rowH_ > 0;
  const int headW = big ? std::clamp(headCols_, 1, width) : width;
  const int visible =
      big ? std::max(height * rowH_ / (bigH_ + rowH_), 1) : std::max(height / 2, 1);
  if (big) {
    for (int k = 0; k < visible; ++k) g.setRowKind(top + 2 * k, RowKind::Big);
    for (int r = top + 2 * visible; r < bottom; ++r) g.setRowKind(r, RowKind::Skip);
  }
  const int first = std::max(0, sel_ - visible + 1);
  for (int k = 0; k < visible; ++k) {
    int idx = first + k;
    if (idx >= static_cast<int>(rows_.size())) break;
    const FeedRow& fr = rows_[idx];
    int row = top + 2 * k;
    g.putText(0, row, fr.line1, fr.fg1, t.bg, headW);
    if (row + 1 < bottom) g.putText(0, row + 1, fr.line2, fr.fg2, t.bg, width);
    if (idx != sel_) continue;
    if (!big) {
      g.outline(0, row, width - 1, std::min(row + 1, bottom - 1), t.selectBg);
      continue;
    }
    // The head row is narrower, so each row gets its own sides.
    for (int c = 0; c < headW; ++c)
      g.addEdges(c, row, kEdgeTop | (c == 0 ? kEdgeLeft : 0) | (c == headW - 1 ? kEdgeRight : 0),
                 t.selectBg);
    if (row + 1 < bottom)
      for (int c = 0; c < width; ++c)
        g.addEdges(c, row + 1,
                   kEdgeBottom | (c == 0 ? kEdgeLeft : 0) | (c == width - 1 ? kEdgeRight : 0),
                   t.selectBg);
  }
}

}  // namespace rc
