#pragma once

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/room_layout.h"
#include "ui/theme.h"

namespace rc {

// Scrollable room log. Following shows the newest lines; otherwise one
// message is selected and the view keeps it on screen.
class LogView {
 public:
  static constexpr int kBarCols = 1;
  // Keeps the selected message if it is still present, else follows.
  void setLines(std::vector<DisplayLine> lines);
  // From following, selects the newest message; then steps to older ones.
  void up();
  // Steps to newer messages; past the newest, returns to following.
  void down();
  // True when the oldest message is selected (or there are none).
  bool atTop() const;
  bool following() const { return selected_.empty(); }
  void follow() { selected_.clear(); }
  std::string selectedId() const { return selected_; }
  // The number of distinct message ids (dividers aside).
  int messageCount() const;
  // Selects the message at `ordinal` among distinct ids, oldest first; false
  // when out of range.
  bool selectMessage(int ordinal);
  // Draws rows [top, bottom), leaving rightReserve columns on the right alone.
  // The last kBarCols columns before those hold the scroll bar, so lines are
  // laid out that much narrower.
  void render(Grid& g, int top, int bottom, int rightReserve, const Theme& t) const;

 private:
  int firstLineOf(const std::string& id) const;
  int lastLineOf(const std::string& id) const;
  int ordinalOf(const std::string& id) const;

  std::vector<DisplayLine> lines_;
  std::string selected_;
};

}  // namespace rc
