#include "input/double_tap.h"

namespace rc {

bool DoubleTap::feed(bool down, uint32_t ms) {
  const bool pressed = down && !wasDown_;
  wasDown_ = down;
  if (!pressed) return false;
  if (armed_ && ms - firstMs_ <= kDoubleTapMs) {
    armed_ = false;
    return true;
  }
  armed_ = true;
  firstMs_ = ms;
  return false;
}

void DoubleTap::reset() {
  armed_ = false;
}

}  // namespace rc
