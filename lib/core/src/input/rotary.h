#pragma once

#include <cstdint>

namespace rc {

// Detents a fast spin may queue up before the rest are dropped.
constexpr int kRotaryQueueMax = 8;

// A quadrature wheel (the T-LoRa Pager's): each sample is the line state
// (B << 1) | A, with 1 meaning the line is low (active). Four valid
// transitions make one detent; an impossible jump (both lines at once, a
// bounce) counts for nothing. take() hands detents out one at a time,
// +1 or -1; a turn the other way cancels queued ones first.
class Rotary {
 public:
  void reset(uint8_t ab);
  void sample(uint8_t ab);
  int take();  // +1, -1 or 0

 private:
  uint8_t prev_ = 0;
  int8_t acc_ = 0;
  int queued_ = 0;
};

}  // namespace rc
