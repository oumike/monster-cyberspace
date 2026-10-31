#include "power/battery_meter.h"

#include <algorithm>
#include <cmath>

namespace rc {

namespace {
// Each reading moves the average a fifth of the way: about 10 s to settle.
constexpr float kWeight = 0.2f;
}  // namespace

void BatteryMeter::feed(int rawPct, int64_t nowMs) {
  sampled_ = true;
  lastMs_ = nowMs;
  if (rawPct < 0) {
    if (++misses_ >= kUnknownAfter) shown_ = -1;
    return;
  }
  misses_ = 0;
  const float raw = float(std::min(rawPct, 100));
  if (shown_ < 0) {
    avg_ = raw;
    shown_ = int(raw);
    return;
  }
  avg_ += (raw - avg_) * kWeight;
  if (std::fabs(avg_ - float(shown_)) >= 1.0f)
    shown_ = std::clamp(int(std::lround(avg_)), 0, 100);
}

}  // namespace rc
