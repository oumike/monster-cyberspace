#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

constexpr uint32_t kClickHoldMs = 2000, kClickMotionQuietMs = 160;

// A trackball click: a tap selects, a long hold turns the screen off, and a
// tap made while the ball is rolling waits until it stops, so rolling onto a
// tile and clicking doesn't open the one passed on the way.
class ClickTimer {
 public:
  void motion(uint32_t ms);  // a trackball move was emitted
  void press(uint32_t ms);
  void release(uint32_t ms);
  // Select: a tap, released before the hold time, once motion has been quiet
  // kClickMotionQuietMs (a deferred tap fires on the first poll after that).
  // ScreenOff: once, at kClickHoldMs of holding; that hold's release emits nothing.
  Action poll(uint32_t ms);  // None, Select or ScreenOff

 private:
  bool down_ = false, held_ = false, tap_ = false, offPending_ = false, moved_ = false;
  uint32_t pressMs_ = 0, motionMs_ = 0;
};

}  // namespace rc
