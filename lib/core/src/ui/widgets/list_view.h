#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A colour role, not a colour: items take the theme's colour when drawn, so a
// theme change applies to menus already open.
// Dim: a heading row (Search's groups), drawn in the theme's dim colour.
enum class ItemTone : uint8_t { Normal, Error, Dim };

struct ListItem {
  std::u32string label, right;
  ItemTone tone = ItemTone::Normal;
  bool marker = false;  // drawn as "•" before the label
  // A second line, shown only by ButtonPad buttons (dim, cut to one line).
  std::u32string detail;
};

// A vertical menu that scrolls to keep the selection visible. Up and Down wrap.
// With columns > 1 the items fill a grid across then down, each cell showing
// its label with `right` at the cell's end; Up and Down move a row (wrapping
// within the column), left and right a column.
class ListView {
 public:
  void setItems(std::vector<ListItem> items);
  void setColumns(int n) { cols_ = n < 1 ? 1 : n; }
  int columns() const { return cols_; }
  void up();
  void down();
  // A column over; false (and no move) at the edge or with one column.
  bool left();
  bool right();
  // In the first column, or with nothing to the right.
  bool atLeftEdge() const { return sel_ % cols_ == 0; }
  bool atRightEdge() const;
  int selected() const { return sel_; }
  void setSelected(int i);
  // Picks item `i`; refuses (and returns false) when out of range.
  bool select(int i);
  // The list a tap Row target belongs to. Default Main; Overlay uses Menu.
  void setListId(ListId id) { listId_ = id; }
  size_t size() const { return items_.size(); }
  const std::vector<ListItem>& items() const { return items_; }
  // Draws rows [top, bottom) in columns [left, left + width).
  void render(Grid& g, int top, int bottom, int left, int width, const Theme& t) const;

 private:
  std::vector<ListItem> items_;
  int sel_ = 0;
  int cols_ = 1;
  ListId listId_ = ListId::Main;
};

}  // namespace rc
