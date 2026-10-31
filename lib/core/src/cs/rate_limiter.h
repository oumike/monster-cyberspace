#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace rc {

// Sliding-log limiter over one or more windows. An acquire at time t counts
// against a window while now - t < periodMs.
class RateLimiter {
 public:
  struct Window {
    uint32_t limit;
    int64_t periodMs;
  };

  explicit RateLimiter(std::vector<Window> w);

  // Records the acquire only when every window allows it.
  bool tryAcquire(int64_t nowMs);
  // 0 when an acquire would succeed now.
  int64_t retryAfterMs(int64_t nowMs) const;

 private:
  std::vector<Window> windows_;
  std::deque<int64_t> log_;
  size_t maxKeep_ = 0;
};

RateLimiter makeCircSendLimiter();
RateLimiter makePresenceLimiter();
RateLimiter makeDeleteLimiter();
RateLimiter makeFlagLimiter();
RateLimiter makeMarkReadLimiter();
RateLimiter makeCmailSendLimiter();
RateLimiter makeStartConversationLimiter();
RateLimiter makeTypingLimiter();
RateLimiter makeEntryLimiter();
RateLimiter makeReplyLimiter();
RateLimiter makeBookmarkLimiter();
RateLimiter makeWatchLimiter();
// Join, promote and leave share it.
RateLimiter makeMembershipLimiter();
// Follows and unfollows each have their own.
RateLimiter makeFollowLimiter();
// One poke an hour, eight a day, across everyone.
RateLimiter makePokeLimiter();

}  // namespace rc
