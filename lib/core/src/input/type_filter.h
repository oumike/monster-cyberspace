#pragma once
#include <string>
#include <string_view>

#include "input/key_event.h"

namespace rc {

// Type-to-filter for a list on a board with a keyboard: letters typed narrow
// the list to names containing them, ignoring case.
class TypeFilter {
 public:
  // Char adds to the filter, Backspace removes the last character, Back
  // clears it. True when the key was used; Back and Backspace pass through
  // while the filter is empty.
  bool key(const KeyEvent& k);
  bool active() const { return !text_.empty(); }
  const std::u32string& text() const { return text_; }
  bool matches(std::u32string_view name) const;
  void clear() { text_.clear(); }

 private:
  std::u32string text_;
};

}  // namespace rc
