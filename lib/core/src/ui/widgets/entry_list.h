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
  void setRows(std::vector<FeedRow> rows);
  void up();
  void down();
  int selected() const { return sel_; }
  void setSelected(int i);  // clamps
  size_t size() const { return rows_.size(); }
  bool atEnd() const;  // the last row is selected
  // Draws rows [top, bottom) across the full grid width.
  void render(Grid& g, int top, int bottom, const Theme& t) const;

 private:
  std::vector<FeedRow> rows_;
  int sel_ = 0;
};

}  // namespace rc
