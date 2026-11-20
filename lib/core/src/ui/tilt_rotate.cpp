// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/tilt_rotate.h"

#include <cmath>

#include "ui/scale2x.h"

namespace rc {

int TiltRotator::pick(float gx, float gy, float gz) {
  const float plane = std::sqrt(gx * gx + gy * gy);
  const float total = std::sqrt(gx * gx + gy * gy + gz * gz);
  if (total <= 0.f || plane < kFlatRatio * total) return -1;
  const float minCos = std::cos(kAxisDeg * 3.14159265f / 180.f);
  int best = -1;
  float bestCos = minCos;
  for (uint8_t r = 0; r < 4; ++r) {
    // The frame's down, one row step, in native axes.
    const scale2x::Pt a = scale2x::frameToNative(0, 0, 3, 3, r);
    const scale2x::Pt b = scale2x::frameToNative(0, 1, 3, 3, r);
    const float c = (float(b.x - a.x) * gx + float(b.y - a.y) * gy) / plane;
    if (c >= bestCos) {
      bestCos = c;
      best = r;
    }
  }
  return best;
}

int TiltRotator::update(float gx, float gy, float gz, uint32_t nowMs) {
  const int p = pick(gx, gy, gz);
  if (p < 0 || p == settled_) {
    candidate_ = -1;
    return settled_;
  }
  if (p != candidate_) {
    candidate_ = p;
    since_ = nowMs;
  } else if (nowMs - since_ >= kSettleMs) {
    settled_ = p;
    candidate_ = -1;
  }
  return settled_;
}

void TiltRotator::reset() {
  settled_ = -1;
  candidate_ = -1;
  since_ = 0;
}

}  // namespace rc
