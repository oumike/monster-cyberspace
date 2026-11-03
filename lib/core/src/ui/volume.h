#pragma once

#include <cstdint>

namespace rc {

// The alert sound's volume, in percent, on boards that can set it
// (board::hasVolume). Off is Settings::buzzer, kept apart so turning the sound
// back on keeps the level.
constexpr uint8_t kVolumeSteps[] = {20, 40, 60, 80, 100};
constexpr uint8_t kVolumeDefault = 60;

constexpr bool validVolume(int v) {
  for (uint8_t s : kVolumeSteps)
    if (v == s) return true;
  return false;
}

// The nearest step; anything out of range is the default.
constexpr uint8_t snapVolume(int v) {
  if (v <= 0 || v > 100) return kVolumeDefault;
  uint8_t best = kVolumeSteps[0];
  for (uint8_t s : kVolumeSteps)
    if ((v > s ? v - s : s - v) < (v > best ? v - best : best - v)) best = s;
  return best;
}

// The tone's amplitude against the board's own level, which is the default:
// squared, so the steps sound evenly spaced (20% ~ 0.11, 100% ~ 2.8).
constexpr float volumeGain(uint8_t pct) {
  const float r = float(pct) / float(kVolumeDefault);
  return r * r;
}

}  // namespace rc
