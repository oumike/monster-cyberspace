#include "net/ap_fallback.h"

namespace rc {

ApFallback::Act ApFallback::update(int64_t nowMs, bool connected, bool apUp, int apClients) {
  if (!started_ || connected) {
    started_ = true;
    lastUp_ = nowMs;
  }
  if (connected) {
    if (apUp && auto_ && apClients == 0) {
      auto_ = false;
      return Act::Drop;
    }
    return Act::None;
  }
  if (!apUp && nowMs - lastUp_ >= kAfterMs) {
    auto_ = true;
    return Act::Raise;
  }
  return Act::None;
}

void ApFallback::noteManual(int64_t nowMs, bool apOn) {
  auto_ = false;
  if (!apOn) {
    started_ = true;
    lastUp_ = nowMs;
  }
}

}  // namespace rc
