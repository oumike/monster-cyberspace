#pragma once

#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"

namespace rc {

// A menu as a grid of outlined buttons, for touch-only boards: three columns
// on a panel at least 800 px wide, else two. Scrolled by whole button rows; a
// drag's Up/Down steps add up to one row's pitch. Each button shows the item's
// label, with its `right` text at the right edge.
class ButtonPad {
 public:
  // Touch and no keys: menus are buttons instead of lines.
  static bool active();
  static int columns();

  void reset() { scroll_ = dragSteps_ = 0; }
  // Up/Down scroll. A tap on a button returns its index; anything else -1.
  int key(const KeyEvent& k);
  // Over grid rows [top, bottom). keepRight shortens the label rather than
  // dropping `right` when both don't fit. columns 0: columns().
  void render(Grid& g, const Theme& t, const std::vector<ListItem>& items, int top, int bottom,
              bool keepRight, int columns = 0);

 private:
  int scroll_ = 0;
  int dragSteps_ = 0;
};

}  // namespace rc
