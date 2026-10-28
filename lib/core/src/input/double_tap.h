#pragma once

#include <cstdint>

namespace rc {

constexpr uint32_t kDoubleTapMs = 500;  // between the two touches going down

// Wakes a dark screen on boards that ask for a double tap: feed it every touch
// sample (down or not) and it is true on the second touch that goes down
// within kDoubleTapMs of the first.
class DoubleTap {
 public:
  bool feed(bool down, uint32_t ms);
  void reset();

 private:
  bool wasDown_ = false, armed_ = false;
  uint32_t firstMs_ = 0;
};

}  // namespace rc
