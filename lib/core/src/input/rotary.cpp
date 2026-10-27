#include "input/rotary.h"

namespace rc {
namespace {

// Indexed by (previous << 2) | current, from camillia-mt's kRotaryDelta.
constexpr int8_t kDelta[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

}  // namespace

void Rotary::reset(uint8_t ab) {
  prev_ = ab & 3;
  acc_ = 0;
  queued_ = 0;
}

void Rotary::sample(uint8_t ab) {
  ab &= 3;
  acc_ += kDelta[(prev_ << 2) | ab];
  prev_ = ab;
  int step = 0;
  if (acc_ >= 4 || acc_ <= -4) {
    step = acc_ > 0 ? 1 : -1;
    acc_ = 0;
  } else if (ab == 0) {
    // Back at rest: a detent that lost a transition or two still counts.
    if (acc_ >= 2) step = 1;
    else if (acc_ <= -2) step = -1;
    acc_ = 0;
  }
  if (!step) return;
  queued_ += step;
  if (queued_ > kRotaryQueueMax) queued_ = kRotaryQueueMax;
  if (queued_ < -kRotaryQueueMax) queued_ = -kRotaryQueueMax;
}

int Rotary::take() {
  if (queued_ > 0) {
    --queued_;
    return 1;
  }
  if (queued_ < 0) {
    ++queued_;
    return -1;
  }
  return 0;
}

}  // namespace rc
