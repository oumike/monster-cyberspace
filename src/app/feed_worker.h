#pragma once

#if !RC_FAKE_API

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "app/messages.h"
#include "cs/cs_api.h"

namespace rc {

// The Feed side of the network task: serves the Feed commands, refreshes the
// feed every 2 minutes while it is on screen, and turns Feed notifications
// into alerts. Writes come only from commands; nothing is retried.
class FeedWorker {
 public:
  explicit FeedWorker(CsApi& api);
  void handle(const Command& c);             // the Feed commands only
  void tick(int64_t monoMs, bool screenOn);  // auto-refresh
  // A Feed-type notification from watch(); dedupes by id. With buzzer false
  // the alert never asks for a beep.
  void onNotice(const Mention& m, bool buzzer);
  void signOut();  // lockedFeed store.clear(), notices cleared

  static bool isFeedCommand(CmdType t);

 private:
  void openFeed();
  void refreshFirst(bool report, bool reloadBookmarks);
  void more();
  void loadBookmarkMap();
  void openEntry(const Command& c);
  void fetchNewReplies(const std::string& postId);
  void moreReplies();
  void postEntry(const Command& c);
  void postReply(const Command& c);
  void remove(const Command& c, bool reply);
  void flag(const Command& c, bool reply);
  void bookmark(const Command& c);
  void unbookmark(const Command& c);
  void watch(const Command& c, bool on);
  void openWatched(bool append);
  void openBookmarks(bool append);
  bool resolveSide(const std::vector<std::string>& postIds, const std::string& next, bool append);
  void setLoading(bool on);
  std::string openPostId();

  CsApi& api_;
  bool feedOpen_ = false;
  int64_t nextRefresh_ = 0;
  std::set<std::string> seen_;  // notification ids already alerted
};

}  // namespace rc

#endif
