#pragma once

#include <cstdint>

namespace rc {

// Steadies the battery percent the UI shows. A raw reading (one ADC sample
// on most boards) jitters by a few points; shown as is it flickers. The
// meter takes a reading at most every kSampleMs, smooths them, and moves the
// shown value only once the average is a whole point away from it.
class BatteryMeter {
 public:
  static constexpr int64_t kSampleMs = 2000;
  // Readings of -1 in a row before the shown value becomes unknown.
  static constexpr int kUnknownAfter = 3;

  // True when a new reading is wanted (the first call, then every kSampleMs).
  bool due(int64_t nowMs) const { return !sampled_ || nowMs - lastMs_ >= kSampleMs; }
  // Feeds a raw percent (-1 when unknown) read at nowMs.
  void feed(int rawPct, int64_t nowMs);
  // The percent to show, 0..100, or -1 when unknown.
  int shown() const { return shown_; }

 private:
  bool sampled_ = false;
  int64_t lastMs_ = 0;
  float avg_ = 0;
  int shown_ = -1;
  int misses_ = 0;
};

}  // namespace rc
