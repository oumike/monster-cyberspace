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
// and Cancel close it. 0 is labelled zeroLabel, the rest "N%".
// openChoices instead makes one bar segment a named stop: value() is the
// stop's index, filled up to and including it, labelled with its name.
class Slider {
 public:
  enum class Result : uint8_t { None, Changed, Picked, Closed };

  void open(std::u32string title, int value, int max, int step, std::u32string zeroLabel);
  void openChoices(std::u32string title, std::vector<std::u32string> names, int sel);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int value() const { return value_; }

  Result key(const KeyEvent& k);
  void render(Grid& g, int top, int bottom, const Theme& t) const;

 private:
  int steps() const { return !names_.empty() ? int(names_.size()) : step_ > 0 ? max_ / step_ : 0; }
  std::u32string label() const;

  std::u32string title_, zero_;
  std::vector<std::u32string> names_;
  int value_ = 0, max_ = 100, step_ = 10;
  bool open_ = false;
};

}  // namespace rc
