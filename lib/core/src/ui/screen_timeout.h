#pragma once

#include <cstdint>
#include <string>

namespace rc {

// The screen timeout, after platy-aprs: with no input for the timeout the
// backlight dims for the last kDimMs, then the screen locks (the screensaver
// or screen off). 0 is Never.
constexpr uint32_t kScreenTimeoutSecs[] = {15, 30, 60, 120, 300, 600, 1800, 3600, 0};
constexpr int64_t kDimMs = 10000;
constexpr uint8_t kDimLevel = 64;    // 25% backlight while dimmed
constexpr uint8_t kSaverLevel = 51;  // 20% backlight for the screensaver

bool validScreenTimeout(uint32_t secs);
// The next option, wrapping; an unlisted value gives the first.
uint32_t nextScreenTimeout(uint32_t secs);
std::string screenTimeoutLabel(uint32_t secs);  // "30 s", "2 min", "Never"

enum class IdlePhase : uint8_t { Active, Dim, Lock };
IdlePhase idlePhase(int64_t idleMs, uint32_t timeoutSecs);

// A position that drifts one step a time and bounces off the edges of
// [0, maxX] x [0, maxY]: the screensaver's monster.
struct Bouncer {
  int x = 0, y = 0, dx = 1, dy = 1;
  void step(int maxX, int maxY);
};

}  // namespace rc
