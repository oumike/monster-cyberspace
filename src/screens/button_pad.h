#pragma once

#include <vector>

#include "input/key_event.h"
#include "ui/button_grid.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"

namespace rc {

// A menu as a grid of outlined buttons, for touch-only boards: three columns
// on a panel at least 800 px wide, else two. Scrolled by whole button rows; a
// drag's Up/Down steps add up to one row's pitch. Each button shows the item's
// label, with its `right` text at the right edge; when any item has a
// detail, every button is a row taller and shows it dim under the label.
class ButtonPad {
 public:
  // Touch and no keys: menus are buttons instead of lines.
  static bool active();
  static int columns();

  void reset() { scroll_ = dragSteps_ = shift_ = selected_ = 0; }
  // With a cursor (touch boards that also have keys) one button is selected:
  // the arrows move it through the grid, Select returns it, and the view
  // scrolls to keep it shown. Without one, Up/Down scroll.
  void setCursor(bool on) { cursor_ = on; }
  // A tap on a button returns its index; anything else -1.
  int key(const KeyEvent& k);
  // The cursor is in the first column, where Left leaves the grid.
  bool atLeftEdge() const { return selected_ % cols_ == 0; }
  // Over grid rows [top, bottom). keepRight shortens the label rather than
  // dropping `right` when both don't fit. columns 0: columns(). header:
  // rows the caller draws above the buttons, from `top`; they scroll away a
  // row per drag step before the buttons scroll.
  void render(Grid& g, const Theme& t, const std::vector<ListItem>& items, int top, int bottom,
              bool keepRight, int columns = 0, int header = 0);
  // Header rows scrolled off the top: the caller draws header row i at
  // top + i - headerShift(), when that is at or below top.
  int headerShift() const { return shift_; }

 private:
  int height_ = kButtonRows;
  int scroll_ = 0;
  int dragSteps_ = 0;
  int shift_ = 0;
  int header_ = 0;
  bool cursor_ = false;
  int selected_ = 0;
  int count_ = 0;
  int cols_ = 2;
};

}  // namespace rc
