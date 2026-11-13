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
  void markOpen();

 private:
  void openFeed();
  void refreshFirst(bool report, bool reloadBookmarks);
  void more();
  void loadBookmarkMap();
  void openEntry(const Command& c);
  void markNoticeRead(const std::string& id);  // PATCH, then off the Unread list
  void fetchNewReplies(const std::string& postId);
  void moreReplies();
  void postEntry(const Command& c);
  void postReply(const Command& c);
  void remove(const Command& c, bool reply);
  void editEntry(const Command& c);
  void editReply(const Command& c);
  void flag(const Command& c, bool reply);
  void bookmark(const Command& c);
  void unbookmark(const Command& c);
  void watch(const Command& c, bool on);
  void openWatched(bool append);
  void openBookmarks(bool append);
  void openTopics();
  void openTopic(const std::string& name, bool append);
  // replyIds: parallel to postIds (Bookmarks only), the bookmarked reply or "".
  bool resolveSide(const std::vector<std::string>& postIds, const std::string& next, bool append,
                   const std::vector<std::string>& replyIds = {});
  void openGuilds(bool append);
  void openGuild(const std::string& slug);
  void guildThreadsMore(const std::string& slug);
  void openMembers(const std::string& slug, bool append);
  void postGuildThread(const Command& c);
  void guildMembership(const Command& c);
  void refreshGuildState();
  void setLoading(bool on);
  std::string openPostId();
#if RC_FEAT_NOTES
  void openNotes(bool append);
  void openNote(const std::string& id, int revision);
  void openNoteHistory(const std::string& id, bool append);
  void saveNote(const Command& c);
  void deleteNote(const std::string& id);
  void clearNotes();
#endif
#if RC_FEAT_PROGRAMS
  void openPrograms(bool more);
  void openProgram(const std::string& id, int release);
  void clearPrograms();
#endif
#if RC_FEAT_SEARCH
  void search(const std::string& typed);
  void searchMore(const std::string& type, bool next);
  void clearSearch();
#endif

  CsApi& api_;
  bool feedOpen_ = false;
  int64_t nextRefresh_ = 0;
  std::set<std::string> seen_;  // notification ids already alerted
  std::set<std::string> seenSocial_;  // the same for isSocialNotice types
};

}  // namespace rc

#endif
