#pragma once

#include <cstdint>

namespace rc {

// Gesture thresholds (spec §3.2).
constexpr int kTapSlopPx = 10;
constexpr uint32_t kLongPressMs = 500;
constexpr int kSwipeMinPx = 40;        // travel must exceed this…
constexpr uint32_t kSwipeMaxMs = 300;  // …in less than this
constexpr int kEdgeBackPx = 16;        // edge swipe starts at x < this

enum class GestureKind : uint8_t { None, Tap, LongPress, Drag, Swipe, EdgeBack };
enum class SwipeDir : uint8_t { Up, Down, Left, Right };

struct Gesture {
  GestureKind kind = GestureKind::None;
  int16_t x = 0, y = 0;  // where the finger went down (every kind)
  int16_t dy = 0;        // Drag: movement since the previous Drag, or since the down point
  SwipeDir dir = SwipeDir::Up;
};

// Turns a stream of touch samples into gestures. One Gestures per touch panel.
class Gestures {
 public:
  // One touch sample (down = finger on glass). True when it completes a gesture.
  bool feed(bool down, int16_t x, int16_t y, uint32_t ms, Gesture& out);
  // Forgets the touch in progress; a finger already down is ignored until it lifts.
  void reset();

 private:
  bool active_ = false;   // a touch is being tracked
  bool armed_ = true;     // false after reset() until a sample reports no finger
  bool moved_ = false;    // past the slop in any axis
  bool dragging_ = false;  // past the slop vertically
  bool longFired_ = false;
  int16_t downX_ = 0, downY_ = 0;
  int16_t dragAnchor_ = 0;
  uint32_t downMs_ = 0;
};

}  // namespace rc
