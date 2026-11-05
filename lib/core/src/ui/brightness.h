#pragma once

#include <cstdint>

namespace rc {

// The screen's brightness, in percent of the board's full backlight: 10 to
// 100 in steps of 10. Not on e-paper (board::eink), which has no backlight.
constexpr uint8_t kBrightnessStep = 10;
constexpr uint8_t kBrightnessMin = 10;
constexpr uint8_t kBrightnessMax = 100;
constexpr uint8_t kBrightnessDefault = 100;

constexpr bool validBrightness(int v) {
  return v >= kBrightnessMin && v <= kBrightnessMax && v % kBrightnessStep == 0;
}

// The nearest step; anything out of range is the default.
constexpr uint8_t snapBrightness(int v) {
  if (v <= 0 || v > kBrightnessMax) return kBrightnessDefault;
  const int s = (v + kBrightnessStep / 2) / kBrightnessStep * kBrightnessStep;
  return uint8_t(s < kBrightnessMin ? kBrightnessMin : s);
}

// What board::backlightLevel gets for the UI's level (255 lit, lower for the
// dimmed screen and the screensaver, 0 off) at pct brightness: lit is pct of
// full, and the lower levels never come out brighter than lit.
constexpr uint8_t backlightFor(uint8_t level, uint8_t pct) {
  if (level == 0) return 0;
  const int full = (255 * snapBrightness(pct) + 50) / 100;
  return uint8_t(level < full ? level : full);
}

}  // namespace rc
