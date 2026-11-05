#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// How bright a board's status LEDs are (board::hasLedLevel): one level for
// all of them. The values are saved: never reorder.
enum class LedLevel : uint8_t { Off, Dim, Low, Medium, High };
constexpr int kLedLevelCount = 5;

// Each colour channel's value at the level. Medium is the 0x20 the LEDs
// had before the setting.
constexpr uint8_t ledLevelValue(LedLevel l) {
  switch (l) {
    case LedLevel::Dim: return 0x02;
    case LedLevel::Low: return 0x08;
    case LedLevel::Medium: return 0x20;
    case LedLevel::High: return 0x80;
    default: return 0;
  }
}

constexpr const char* kLedLevelKeys[] = {"off", "dim", "low", "medium", "high"};
constexpr const char32_t* kLedLevelLabels[] = {U"Off", U"Dim", U"Low", U"Medium", U"High"};

constexpr const char* ledLevelKey(LedLevel l) {
  return uint8_t(l) < kLedLevelCount ? kLedLevelKeys[uint8_t(l)] : "medium";
}
constexpr const char32_t* ledLevelLabel(LedLevel l) {
  return uint8_t(l) < kLedLevelCount ? kLedLevelLabels[uint8_t(l)] : U"Medium";
}
// Out of range gives fallback.
constexpr LedLevel ledLevelFromIndex(int i, LedLevel fallback = LedLevel::Medium) {
  return i >= 0 && i < kLedLevelCount ? LedLevel(i) : fallback;
}
// out unchanged on failure.
constexpr bool parseLedLevel(std::string_view key, LedLevel& out) {
  for (int i = 0; i < kLedLevelCount; ++i)
    if (key == kLedLevelKeys[i]) {
      out = LedLevel(i);
      return true;
    }
  return false;
}

}  // namespace rc
