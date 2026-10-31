#pragma once

#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/overlay.h"

namespace rc {

// What boards that can't type (board::canType) open instead of a text field:
// the user's canned phrases, then "Emoji…", which hands over to the emoji
// tray. Up/Down move, Select picks, Back cancels.
class CannedPicker {
 public:
  enum class Kind : uint8_t {
    None,    // still open
    Send,    // send `text`; closed
    Emoji,   // open the emoji tray; closed
    Closed,  // cancelled
  };
  struct Out {
    Kind kind = Kind::None;
    std::u32string text;
  };

  void open(const std::vector<std::string>& phrases);
  void close() { box_.close(); }
  bool isOpen() const { return box_.isOpen(); }
  int rows() const { return int(phrases_.size()) + 1; }
  Out key(const KeyEvent& k);
  void render(Grid& g, int top, int bottom, const Theme& t) const;

 private:
  Overlay box_;
  std::vector<std::u32string> phrases_;
};

}  // namespace rc
