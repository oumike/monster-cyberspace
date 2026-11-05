#pragma once

#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A centred box with a horizontal bar for a value from 0 to max in steps:
// Left/Down lower it, Right/Up raise it, Select keeps it, Back cancels. On
// touch, a tap on the bar sets the value under it, "-" and "+" step, and Set
// and Cancel close it. 0 is labelled zeroLabel, the rest "N%". min, when
// given, is the lowest value it goes to.
// openChoices instead draws a wide line with a dot per named stop, spread
// from left to right, each name under its dot; value() is the chosen stop's
// index, its dot filled. A tap on a dot or its name picks it.
class Slider {
 public:
  enum class Result : uint8_t { None, Changed, Picked, Closed };

  void open(std::u32string title, int value, int max, int step, std::u32string zeroLabel, int min = 0);
  void openChoices(std::u32string title, std::vector<std::u32string> names, int sel);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int value() const { return value_; }

  Result key(const KeyEvent& k);
  void render(Grid& g, int top, int bottom, const Theme& t) const;

 private:
  int steps() const { return !names_.empty() ? int(names_.size()) : step_ > 0 ? max_ / step_ : 0; }
  std::u32string label() const;
  void renderStops(Grid& g, int top, int bottom, const Theme& t) const;

  std::u32string title_, zero_;
  std::vector<std::u32string> names_;
  int value_ = 0, min_ = 0, max_ = 100, step_ = 10;
  bool open_ = false;
};

}  // namespace rc
