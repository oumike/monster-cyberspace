#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "feed/feed_store.h"
#include "guild/guild_store.h"
#include "ui/notice_text.h"

namespace rc {

// An unread Feed notification: id is the notification id. user is set for a
// follower or a poke (Select opens the profile), else postId.
struct FeedNotice {
  std::string id, postId, text;
  int64_t at = 0;  // epoch ms; 0 unknown
  std::string user;
  bool social = false;  // ui/notice_text isSocialNotice
};

// Everything the Feed screens read and the network side writes.
struct FeedState {
  FeedStore store;
  GuildStore guilds;
  std::vector<FeedNotice> notices;  // unread Feed notifications, newest last
  std::vector<Topic> topics;        // GET /v1/topics, most entries first
  bool topicsLoaded = false;
  bool loading = false;
  // Settings' Unread filter, which the network side reads with each poll.
  UnreadFilter unreadFilter = UnreadFilter::All;
  bool socialDropped = false;  // set with Mentions: FeedWorker forgets the social ids it saw
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
