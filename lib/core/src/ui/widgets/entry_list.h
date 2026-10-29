#pragma once

#include <cstddef>
#include <vector>

#include "ui/feed_layout.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A list of two-row Feed entries that scrolls to keep the selection visible
// and outlines both rows of the selection (like ListView). Up and Down wrap.
class EntryList {
 public:
  // The last column of each row (head rows too) holds the scroll bar, so
  // rows are laid out this much narrower.
  static constexpr int kBarCols = 1;
  void setRows(std::vector<FeedRow> rows);
  void up();
  void down();
  int selected() const { return sel_; }
  void setSelected(int i);  // clamps
  bool select(int i);       // false when out of range
  size_t size() const { return rows_.size(); }
  bool atEnd() const;  // the last row is selected
  // Draws each entry's first line as a RowKind::Big row cut to headCols, and
  // fits as many entries as rowH and bigH pixel rows allow; rows left over
  // are skipped. bigH 0 (the default) draws two Normal rows an entry.
  void setBigHeads(int rowH, int bigH, int headCols);
  // Draws rows [top, bottom) across the full grid width.
  void render(Grid& g, int top, int bottom, const Theme& t, int rightReserve = 0) const;

 private:
  std::vector<FeedRow> rows_;
  int sel_ = 0;
  int rowH_ = 0, bigH_ = 0, headCols_ = 0;
};

}  // namespace rc
