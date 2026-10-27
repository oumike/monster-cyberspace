#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chat/presence.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// Who's-here panel overlaid on the right of the room log.
class Drawer {
 public:
  void setPeople(std::vector<PresenceEntry> people, int64_t nowMs, int64_t idleAfterMs);
  void up();
  void down();
  bool select(int i);  // false when out of range
  std::string selectedUsername() const;
  // Draws rows [top, bottom) in the rightmost width(cols, docked) columns.
  void render(Grid& g, int top, int bottom, const Theme& t, bool docked = false) const;
  // 15 cols (120 px in Comfortable, 90 px in Compact), capped at half of a
  // narrow (portrait) grid of `cols` columns. Docked: a fifth of the grid,
  // at least 15 cols.
  static int width(int cols, bool docked = false);
  // A landscape panel at least 480 px wide keeps the drawer docked open in rooms.
  static bool docks(int panelWidthPx, int panelHeightPx);

 private:
  struct Person {
    std::string username;
    bool idle;
  };
  std::vector<Person> people_;
  int sel_ = 0;
};

}  // namespace rc
