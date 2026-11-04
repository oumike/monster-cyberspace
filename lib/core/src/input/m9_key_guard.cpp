#include "input/m9_key_guard.h"

namespace rc {

uint32_t M9KeyGuard::charWindow() const {
  if (!paceMs_) return kMaxMs;
  uint32_t w = paceMs_ * 3 / 5;
  if (w < kMinMs) w = kMinMs;
  if (w > kMaxMs) w = kMaxMs;
  return w;
}

bool M9KeyGuard::accept(uint8_t key, uint32_t nowMs) {
  const uint32_t gap = nowMs - lastMs_;
  if (lastKey_ && key == lastKey_) {
    if (gap < (isChar(key) ? charWindow() : kMinMs)) return false;
  } else if (lastKey_ && isChar(key) && isChar(lastKey_) && gap < kPaceIdleMs) {
    paceMs_ = paceMs_ ? (paceMs_ * 3 + gap) / 4 : gap;
  }
  lastKey_ = key;
  lastMs_ = nowMs;
  return true;
}

}  // namespace rc
