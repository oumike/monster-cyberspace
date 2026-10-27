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
  // Draws rows [top, bottom) in the rightmost width() columns.
  void render(Grid& g, int top, int bottom, const Theme& t) const;
  // 15 cols (120 px in Comfortable, 90 px in Compact), capped at half of a
  // narrow (portrait) grid of `cols` columns.
  static int width(int cols);

 private:
  struct Person {
    std::string username;
    bool idle;
  };
  std::vector<Person> people_;
  int sel_ = 0;
};

}  // namespace rc
