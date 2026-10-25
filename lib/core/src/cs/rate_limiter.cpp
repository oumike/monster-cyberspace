#include "cs/rate_limiter.h"

#include <algorithm>

namespace rc {

RateLimiter::RateLimiter(std::vector<Window> w) : windows_(std::move(w)) {
  for (const Window& win : windows_) maxKeep_ = std::max<size_t>(maxKeep_, win.limit);
}

int64_t RateLimiter::retryAfterMs(int64_t nowMs) const {
  int64_t wait = 0;
  for (const Window& w : windows_) {
    // log_ is ascending; entries after `first` are inside this window.
    auto first = std::upper_bound(log_.begin(), log_.end(), nowMs - w.periodMs);
    size_t inWindow = static_cast<size_t>(log_.end() - first);
    if (inWindow < w.limit) continue;
    if (w.limit == 0) return w.periodMs;
    int64_t frees = *(first + (inWindow - w.limit)) + w.periodMs;
    wait = std::max(wait, frees - nowMs);
  }
  return wait;
}

bool RateLimiter::tryAcquire(int64_t nowMs) {
  if (retryAfterMs(nowMs) > 0) return false;
  log_.push_back(nowMs);
  while (log_.size() > maxKeep_) log_.pop_front();
  return true;
}

RateLimiter makeCircSendLimiter() {
  return RateLimiter({{15, 60000}, {150, 3600000}, {1000, 86400000}});
}

RateLimiter makePresenceLimiter() { return RateLimiter({{15, 60000}}); }

RateLimiter makeDeleteLimiter() { return RateLimiter({{5, 60000}, {30, 86400000}}); }

RateLimiter makeFlagLimiter() {
  return RateLimiter({{5, 60000}, {20, 3600000}, {50, 86400000}});
}

RateLimiter makeMarkReadLimiter() { return RateLimiter({{60, 60000}}); }

}  // namespace rc
