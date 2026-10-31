#pragma once

#include <cstdint>

namespace rc {

// Boards that sleep themselves on battery (the X4): how long idle before it
// sleeps, and how often it wakes to refresh (0: only a button wakes it).
constexpr uint32_t kIdleChoices[] = {60, 120, 300, 600, 1800};
constexpr uint32_t kWakeChoices[] = {0, 5, 15, 30, 60, 120};
bool validIdleSecs(uint32_t secs);
bool validWakeMins(uint32_t mins);

// Below this on battery, held for kCriticalHoldMs, the board powers off.
constexpr float kCriticalVolts = 3.40f;
constexpr uint32_t kCriticalHoldMs = 10'000;
// A timed wake that hasn't refreshed by then sleeps anyway.
constexpr uint32_t kTimedWakeMaxMs = 90'000;

enum class PowerStep { Stay, Sleep, PowerOff };

// When to sleep or power off. On USB: always live. On battery: asleep after
// idleSecs without a button, or, woken by the timer, once the screen is
// refreshed (a button makes it a full wake).
class PowerPolicy {
 public:
  // timedWake: woken by the sleep timer, not a button or power-on.
  void begin(bool timedWake, uint32_t nowMs);
  void input(uint32_t nowMs);
  // A timed wake's refresh is drawn.
  void refreshed(uint32_t nowMs);
  // volts <= 0: unknown.
  PowerStep step(uint32_t nowMs, bool usb, float volts, uint32_t idleSecs);
  bool timedWake() const { return timed_; }

 private:
  bool timed_ = false, refreshed_ = false, critical_ = false;
  uint32_t startMs_ = 0, inputMs_ = 0, criticalMs_ = 0;
};

// The deep-sleep timer for wakeMins; 0: none.
uint64_t wakeTimerUs(uint32_t wakeMins);

}  // namespace rc
