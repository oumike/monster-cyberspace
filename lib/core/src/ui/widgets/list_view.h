#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A colour role, not a colour: items take the theme's colour when drawn, so a
// theme change applies to menus already open.
enum class ItemTone : uint8_t { Normal, Error };

struct ListItem {
  std::u32string label, right;
  ItemTone tone = ItemTone::Normal;
  bool marker = false;  // drawn as "•" before the label
};

// A vertical menu that scrolls to keep the selection visible. Up and Down wrap.
class ListView {
 public:
  void setItems(std::vector<ListItem> items);
  void up();
  void down();
  int selected() const { return sel_; }
  void setSelected(int i);
  size_t size() const { return items_.size(); }
  // Draws rows [top, bottom) in columns [left, left + width).
  void render(Grid& g, int top, int bottom, int left, int width, const Theme& t) const;

 private:
  std::vector<ListItem> items_;
  int sel_ = 0;
};

}  // namespace rc
