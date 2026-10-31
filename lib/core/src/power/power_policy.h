#pragma once

#include <cstdint>

namespace rc {

// Boards that sleep themselves on battery (the X4): how long idle before it
// sleeps, and how often it wakes to refresh (0: only a button wakes it).
constexpr uint32_t kIdleChoices[] = {60, 120, 300, 600, 1800};
constexpr uint32_t kWakeChoices[] = {0, 5, 15, 30, 60, 120};
bool validIdleSecs(uint32_t secs);
bool validWakeMins(uint32_t mins);

}  // namespace rc
