#include "input/click_timer.h"

namespace rc {

void ClickTimer::motion(uint32_t ms) {
  moved_ = true;
  motionMs_ = ms;
}

void ClickTimer::press(uint32_t ms) {
  down_ = true;
  held_ = false;
  pressMs_ = ms;
}

void ClickTimer::release(uint32_t ms) {
  if (!down_) return;
  down_ = false;
  if (held_) return;
  // A hold that ended between polls still counts as a hold.
  if (ms - pressMs_ >= kClickHoldMs) offPending_ = true;
  else tap_ = true;
}

Action ClickTimer::poll(uint32_t ms) {
  if (offPending_ || (down_ && !held_ && ms - pressMs_ >= kClickHoldMs)) {
    offPending_ = false;
    held_ = down_;
    return Action::ScreenOff;
  }
  if (tap_ && (!moved_ || ms - motionMs_ >= kClickMotionQuietMs)) {
    tap_ = false;
    return Action::Select;
  }
  return Action::None;
}

}  // namespace rc
