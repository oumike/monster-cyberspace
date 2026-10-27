#pragma once

#include <vector>

#include "input/gestures.h"
#include "input/key_event.h"
#include "ui/tap_map.h"

namespace rc {

struct TouchOutput {
  std::vector<KeyEvent> keys;
  int oskKey = -1;    // a keyboard key index for Osk::press
  bool hideBanner = false;
};

// Turns a gesture plus the previous frame's TapMap into ordinary KeyEvents.
class TouchResolver {
 public:
  void resolve(const Gesture& g, const TapMap& m, TouchOutput& out);
  void reset();  // drops the drag carry
  // Touch-only boards: in a holdToAct list a tap or long press does nothing
  // and a Hold acts on the item. Elsewhere, and on boards with keys, a Hold
  // does nothing.
  void setTouchOnly(bool on) { touchOnly_ = on; }

 private:
  void stepList(const TapMap& m, ListId list, int steps, bool towardLater,
                TouchOutput& out);
  int carry_ = 0;
  bool touchOnly_ = false;
};

}  // namespace rc
