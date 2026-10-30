#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "feed/feed_store.h"
#include "guild/guild_store.h"

namespace rc {

// An unread Feed notification: id is the notification id.
struct FeedNotice {
  std::string id, postId, text;
};

// Everything the Feed screens read and the network side writes.
struct FeedState {
  FeedStore store;
  GuildStore guilds;
  std::vector<FeedNotice> notices;  // unread Feed notifications, newest last
  bool loading = false;
};

class LockedFeed {
 public:
  // Runs f(FeedState&) under the lock. Keep f short: no I/O, and never
  // feedSource().command() inside it.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  FeedState state_;
};

LockedFeed& lockedFeed();

}  // namespace rc
