#include "input/gestures.h"

#include <cstdlib>

namespace rc {

void Gestures::reset() {
  active_ = false;
  armed_ = false;
  moved_ = false;
  dragging_ = false;
  longFired_ = false;
}

bool Gestures::feed(bool down, int16_t x, int16_t y, uint32_t ms, Gesture& out) {
  // After reset(), ignore a finger already on the glass until it lifts.
  if (!armed_) {
    if (!down) armed_ = true;
    return false;
  }

  if (down) {
    if (!active_) {
      // Finger just went down: start a fresh touch.
      active_ = true;
      moved_ = false;
      dragging_ = false;
      longFired_ = false;
      downX_ = x;
      downY_ = y;
      dragAnchor_ = y;
      downMs_ = ms;
      return false;
    }

    int dxT = x - downX_;
    int dyT = y - downY_;
    if (std::abs(dxT) > kTapSlopPx || std::abs(dyT) > kTapSlopPx) moved_ = true;
    if (!dragging_ && std::abs(dyT) > kTapSlopPx) {
      dragging_ = true;
      dragAnchor_ = downY_;
    }
    if (dragging_) {
      int16_t d = (int16_t)(y - dragAnchor_);
      dragAnchor_ = y;
      out = Gesture{};
      out.kind = GestureKind::Drag;
      out.x = downX_;
      out.y = downY_;
      out.dy = d;
      return true;
    }
    if (!moved_ && !longFired_ && (ms - downMs_) >= kLongPressMs) {
      longFired_ = true;
      out = Gesture{};
      out.kind = GestureKind::LongPress;
      out.x = downX_;
      out.y = downY_;
      return true;
    }
    return false;
  }

  // Finger lifted.
  if (!active_) return false;
  active_ = false;

  if (longFired_ || dragging_) return false;

  int dx = x - downX_;
  int dy = y - downY_;
  uint32_t dur = ms - downMs_;
  bool horizontal = std::abs(dx) >= std::abs(dy);
  int travel = horizontal ? std::abs(dx) : std::abs(dy);
  if (dur < kSwipeMaxMs && travel > kSwipeMinPx) {
    SwipeDir dir = horizontal ? (dx > 0 ? SwipeDir::Right : SwipeDir::Left)
                              : (dy > 0 ? SwipeDir::Down : SwipeDir::Up);
    out = Gesture{};
    out.x = downX_;
    out.y = downY_;
    out.dir = dir;
    if (dir == SwipeDir::Right && downX_ < kEdgeBackPx) {
      out.kind = GestureKind::EdgeBack;
    } else {
      out.kind = GestureKind::Swipe;
    }
    return true;
  }

  bool withinSlop = std::abs(dx) <= kTapSlopPx && std::abs(dy) <= kTapSlopPx;
  if (!moved_ && withinSlop && dur < kLongPressMs) {
    out = Gesture{};
    out.kind = GestureKind::Tap;
    out.x = downX_;
    out.y = downY_;
    return true;
  }
  return false;
}

}  // namespace rc
