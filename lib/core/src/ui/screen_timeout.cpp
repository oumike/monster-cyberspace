#include "ui/screen_timeout.h"

#include <algorithm>

namespace rc {

bool validScreenTimeout(uint32_t secs) {
  for (uint32_t s : kScreenTimeoutSecs)
    if (s == secs) return true;
  return false;
}

uint32_t nextScreenTimeout(uint32_t secs) {
  constexpr size_t n = sizeof(kScreenTimeoutSecs) / sizeof(kScreenTimeoutSecs[0]);
  for (size_t i = 0; i < n; ++i)
    if (kScreenTimeoutSecs[i] == secs) return kScreenTimeoutSecs[(i + 1) % n];
  return kScreenTimeoutSecs[0];
}

std::string screenTimeoutLabel(uint32_t secs) {
  if (secs == 0) return "Never";
  if (secs < 60) return std::to_string(secs) + " s";
  return std::to_string(secs / 60) + " min";
}

IdlePhase idlePhase(int64_t idleMs, uint32_t timeoutSecs) {
  if (timeoutSecs == 0) return IdlePhase::Active;
  const int64_t limit = int64_t(timeoutSecs) * 1000;
  if (idleMs >= limit) return IdlePhase::Lock;
  if (idleMs >= limit - kDimMs) return IdlePhase::Dim;
  return IdlePhase::Active;
}

void Bouncer::step(int maxX, int maxY) {
  auto axis = [](int& p, int& d, int max) {
    if (max <= 0) {
      p = 0;
      return;
    }
    p = std::clamp(p + d, 0, max);
    if (p == 0 || p == max) d = p == 0 ? 1 : -1;
  };
  axis(x, dx, maxX);
  axis(y, dy, maxY);
}

}  // namespace rc
