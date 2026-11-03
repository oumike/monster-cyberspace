#pragma once

#include <cstdint>

namespace rc {

// The alert sound's volume, in percent, on boards that can set it
// (board::hasVolume): 10 to 100 in steps of 10. Off is Settings::buzzer, kept
// apart so turning the sound back on keeps the level.
constexpr uint8_t kVolumeStep = 10;
constexpr uint8_t kVolumeMax = 100;
constexpr uint8_t kVolumeDefault = 60;

constexpr bool validVolume(int v) { return v >= kVolumeStep && v <= kVolumeMax && v % kVolumeStep == 0; }

// The nearest step; anything out of range is the default.
constexpr uint8_t snapVolume(int v) {
  if (v <= 0 || v > kVolumeMax) return kVolumeDefault;
  const int s = (v + kVolumeStep / 2) / kVolumeStep * kVolumeStep;
  return uint8_t(s < kVolumeStep ? kVolumeStep : s);
}

// The tone's amplitude against the board's own level, which is the default:
// squared, so the steps sound evenly spaced (10% ~ 0.03, 100% ~ 2.8).
constexpr float volumeGain(uint8_t pct) {
  const float r = float(pct) / float(kVolumeDefault);
  return r * r;
}

}  // namespace rc
