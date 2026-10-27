#pragma once

#include <cstdint>

namespace rc {

// Gesture thresholds (spec §3.2).
constexpr int kTapSlopPx = 10;
constexpr uint32_t kLongPressMs = 500;
constexpr uint32_t kHoldMs = 1000;     // a Hold follows the LongPress of a still finger
constexpr int kSwipeMinPx = 40;        // travel must exceed this…
constexpr uint32_t kSwipeMaxMs = 300;  // …in less than this
constexpr int kEdgeBackPx = 16;        // the narrowest edge zone, and the zone before setWidth
constexpr uint32_t kEdgeSwipeMaxMs = 800;  // edge swipes may be slower

// EdgeBack: from the left edge rightwards. EdgeOptions: from the right edge leftwards.
enum class GestureKind : uint8_t { None, Tap, LongPress, Drag, Swipe, EdgeBack, EdgeOptions, Hold };
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
  // The panel width in px: each edge zone is an eighth of it (at least
  // kEdgeBackPx). Until set, only the left kEdgeBackPx is an edge.
  void setWidth(int px) { width_ = px; }

 private:
  bool active_ = false;   // a touch is being tracked
  bool armed_ = true;     // false after reset() until a sample reports no finger
  bool moved_ = false;    // past the slop in any axis
  bool dragging_ = false;  // past the slop vertically
  bool longFired_ = false;
  bool holdFired_ = false;
  int8_t edge_ = 0;       // the touch began in the left (-1) or right (+1) edge zone
  int width_ = 0;
  int16_t downX_ = 0, downY_ = 0;
  int16_t dragAnchor_ = 0;
  uint32_t downMs_ = 0;
};

}  // namespace rc
