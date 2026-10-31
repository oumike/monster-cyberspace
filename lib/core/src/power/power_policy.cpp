#include "power/power_policy.h"

namespace rc {

bool validIdleSecs(uint32_t secs) {
  for (uint32_t c : kIdleChoices)
    if (c == secs) return true;
  return false;
}

bool validWakeMins(uint32_t mins) {
  for (uint32_t c : kWakeChoices)
    if (c == mins) return true;
  return false;
}

}  // namespace rc

namespace rc {

void PowerPolicy::begin(bool timedWake, uint32_t nowMs) {
  timed_ = timedWake;
  refreshed_ = critical_ = false;
  startMs_ = inputMs_ = nowMs;
}

void PowerPolicy::input(uint32_t nowMs) {
  timed_ = false;
  inputMs_ = nowMs;
}

void PowerPolicy::refreshed(uint32_t) { refreshed_ = true; }

PowerStep PowerPolicy::step(uint32_t nowMs, bool usb, float volts, uint32_t idleSecs) {
  const bool low = !usb && volts > 0.0f && volts < kCriticalVolts;
  if (!low) {
    critical_ = false;
  } else if (!critical_) {
    critical_ = true;
    criticalMs_ = nowMs;
  } else if (nowMs - criticalMs_ >= kCriticalHoldMs) {
    return PowerStep::PowerOff;
  }
  if (usb) return PowerStep::Stay;
  if (timed_) {
    return refreshed_ || nowMs - startMs_ >= kTimedWakeMaxMs ? PowerStep::Sleep : PowerStep::Stay;
  }
  return nowMs - inputMs_ >= idleSecs * 1000u ? PowerStep::Sleep : PowerStep::Stay;
}

uint64_t wakeTimerUs(uint32_t wakeMins) { return uint64_t(wakeMins) * 60u * 1000000u; }

}  // namespace rc
