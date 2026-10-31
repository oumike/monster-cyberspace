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
