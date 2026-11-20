// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace rc {

// Picks the panel rotation (ui/rotation.h) from gravity, for boards with an
// accelerometer. Gravity is given in the native panel's axes as ui/scale2x
// draws it: x right, y down, z out of the screen, in any unit. A rotation is
// taken once gravity has pointed within kAxisDeg of its "down" for kSettleMs;
// in between, or with the device lying flat, the last one stays.
class TiltRotator {
 public:
  static constexpr float kAxisDeg = 35.f;    // the dead zone between two is 20°
  static constexpr float kFlatRatio = 0.42f;  // in-plane share under ~25° tilt: flat
  static constexpr uint32_t kSettleMs = 500;

  // The rotation gravity settles on, or -1 until one has.
  int update(float gx, float gy, float gz, uint32_t nowMs);
  int settled() const { return settled_; }
  void reset();

  // Which rotation gravity points down in now, ignoring time; -1 if none.
  static int pick(float gx, float gy, float gz);

 private:
  int settled_ = -1;
  int candidate_ = -1;
  uint32_t since_ = 0;
};

}  // namespace rc
