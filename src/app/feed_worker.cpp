
#include "app/feed_worker.h"

#include <esp_timer.h>

#include <algorithm>

#include "app/queues.h"
#include "feed/markdown_text.h"
#include "store/locked_feed.h"

namespace rc {

namespace {

constexpr int64_t kRefreshMs = 120000;
constexpr int kFeedLimit = 10, kReplyLimit = 20, kBookmarkMapLimit = 50, kBookmarkMapPages = 4;
constexpr int kSideLimit = 10;
constexpr size_t kPreviewCodePoints = 80;
constexpr size_t kMaxSeen = 200, kMaxNotices = 50;

int64_t mono() { return esp_timer_get_time() / 1000; }

void post(EvtType t, const std::string& text = "", int code = 0, bool ok = true) {
  postEvent(Event{t, text, code, ok});
}

void banner(const std::string& text, bool ok = false) { post(EvtType::Banner, text, 0, ok); }

std::string errorText(const ApiError& e, bool deleteOrFlag) {
  if (deleteOrFlag && e.http == 403) return "Not allowed";
  if (e.code == "LOCAL_RATE_LIMIT") return e.message;  // "Slow down: try in Ns"
  if (e.http == 429) return "Slow down";
  return e.message;
}

// A failed read. TOO_LARGE already showed its banner in fetchList().
void fail(const ApiError& e) {
  if (e.code != "TOO_LARGE") banner(errorText(e, false));
}

// Every list fetch goes through here: on TOO_LARGE it asks again with a
// smaller page (10 → 5 → 2) and gives up at 0. Each attempt is a read.
template <class F>
ApiError fetchList(int limit, F fetch) {
  for (;;) {
    ApiError e = fetch(limit);
    if (e.code != "TOO_LARGE") return e;
    limit = nextSmallerLimit(limit);
    if (limit <= 0) {
      banner("Too large to load");
      return e;
    }
  }
}

bool endsWith(const std::string& s, const char* tail) {
  std::string t(tail);
  return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

}  // namespace

FeedWorker::FeedWorker(CsApi& api) : api_(api) {}

bool FeedWorker::isFeedCommand(CmdType t) {
  switch (t) {
    case CmdType::OpenFeed:
    case CmdType::CloseFeed:
    case CmdType::FeedMore:
    case CmdType::FeedRefresh:
    case CmdType::OpenEntry:
    case CmdType::MoreReplies:
    case CmdType::PostEntry:
    case CmdType::PostReply:
    case CmdType::DeleteEntry:
    case CmdType::DeleteReply:
    case CmdType::FlagEntry:
    case CmdType::FlagReply:
    case CmdType::Bookmark:
    case CmdType::Unbookmark:
    case CmdType::Watch:
    case CmdType::Unwatch:
    case CmdType::OpenWatched:
    case CmdType::OpenBookmarks:
      return true;
    default:
      return false;
  }
}

void FeedWorker::handle(const Command& c) {
  switch (c.type) {
    case CmdType::OpenFeed: openFeed(); break;
    case CmdType::CloseFeed: feedOpen_ = false; break;
    case CmdType::FeedMore: more(); break;
    case CmdType::FeedRefresh:
      nextRefresh_ = mono() + kRefreshMs;
      refreshFirst(true, true);
      break;
    case CmdType::OpenEntry: openEntry(c); break;
    case CmdType::MoreReplies: moreReplies(); break;
    case CmdType::PostEntry: postEntry(c); break;
    case CmdType::PostReply: postReply(c); break;
    case CmdType::DeleteEntry: remove(c, false); break;
    case CmdType::DeleteReply: remove(c, true); break;
    case CmdType::FlagEntry: flag(c, false); break;
    case CmdType::FlagReply: flag(c, true); break;
    case CmdType::Bookmark: bookmark(c); break;
    case CmdType::Unbookmark: unbookmark(c); break;
    case CmdType::Watch: watch(c, true); break;
    case CmdType::Unwatch: watch(c, false); break;
    case CmdType::OpenWatched: openWatched(c.n == 1); break;
    case CmdType::OpenBookmarks: openBookmarks(c.n == 1); break;
    default: break;
  }
}

void FeedWorker::tick(int64_t monoMs, bool screenOn) {
  if (!feedOpen_ || !screenOn || monoMs < nextRefresh_) return;
  nextRefresh_ = monoMs + kRefreshMs;
  refreshFirst(false, false);
}

// FeedList is showing, even if offline: the next tick once online refreshes.
void FeedWorker::markOpen() {
  feedOpen_ = true;
  nextRefresh_ = 0;
}

void FeedWorker::signOut() {
  feedOpen_ = false;
  seen_.clear();
  lockedFeed().with([](FeedState& s) {
    s.store.clear();
    s.notices.clear();
    s.loading = false;
  });
  post(EvtType::FeedUpdated);
}

void FeedWorker::setLoading(bool on) {
  lockedFeed().with([&](FeedState& s) { s.loading = on; });
}

std::string FeedWorker::openPostId() {
  return lockedFeed().with([](FeedState& s) {
    const Entry* e = s.store.entry();
    return e ? e->id : std::string();
  });
}

// ---- feed list ----

void FeedWorker::openFeed() {
  feedOpen_ = true;
  nextRefresh_ = mono() + kRefreshMs;
  refreshFirst(true, false);
  bool loaded = lockedFeed().with([](FeedState& s) { return s.store.bookmarksLoaded(); });
  if (!loaded) loadBookmarkMap();
}

// The timed refresh doesn't report errors (report=false); the palette's also
// reloads the bookmark map.
void FeedWorker::refreshFirst(bool report, bool reloadBookmarks) {
  std::vector<Entry> page;
  std::string next;
  setLoading(true);
  ApiError e = fetchList(kFeedLimit, [&](int limit) {
    page.clear();
    return api_.feed("", limit, page, next);
  });
  setLoading(false);
  if (e.ok()) {
    // An empty first page would wipe a held list (mergeFirstPage replaces it
    // when nothing overlaps), so it is ignored while the list holds entries.
    lockedFeed().with([&](FeedState& s) {
      if (page.empty() && !s.store.feed().empty()) return;
      s.store.mergeFirstPage(std::move(page), next);
    });
    post(EvtType::FeedUpdated);
  } else if (report || e.code == "TOO_LARGE") {
    fail(e);
  }
  if (reloadBookmarks) loadBookmarkMap();
  // The screen may be waiting on the list either way.
  if (!e.ok()) post(EvtType::FeedUpdated);
}

void FeedWorker::more() {
  std::string cursor = lockedFeed().with([](FeedState& s) { return s.store.feedCursor(); });
  if (cursor.empty()) {
    post(EvtType::FeedUpdated);
    return;
  }
  std::vector<Entry> page;
  std::string next;
  setLoading(true);
  ApiError e = fetchList(kFeedLimit, [&](int limit) {
    page.clear();
    return api_.feed(cursor, limit, page, next);
  });
  setLoading(false);
  if (e.ok()) lockedFeed().with([&](FeedState& s) { s.store.appendPage(std::move(page), next); });
  else fail(e);
  post(EvtType::FeedUpdated);
}

void FeedWorker::loadBookmarkMap() {
  std::vector<Bookmark> all;
  std::string cursor;
  for (int i = 0; i < kBookmarkMapPages; ++i) {
    std::vector<Bookmark> page;
    std::string next;
    ApiError e = fetchList(kBookmarkMapLimit, [&](int limit) {
      page.clear();
      return api_.bookmarks(cursor, page, next, limit);
    });
    if (!e.ok()) {
      fail(e);
      return;  // stays unloaded; the next OpenFeed tries again
    }
    all.insert(all.end(), page.begin(), page.end());
    if (next.empty()) break;
    cursor = next;
  }
  lockedFeed().with([&](FeedState& s) { s.store.setBookmarks(all); });
  post(EvtType::FeedUpdated);
}

// ---- entry and replies ----

void FeedWorker::openEntry(const Command& c) {
  if (!c.b.empty()) {
    api_.markNotificationRead(c.b);
    lockedFeed().with([&](FeedState& s) {
      auto& n = s.notices;
      n.erase(std::remove_if(n.begin(), n.end(), [&](const FeedNotice& x) { return x.id == c.b; }),
              n.end());
    });
  }
  setLoading(true);
  Entry entry;
  ApiError e = api_.entry(c.a, entry);
  if (!e.ok()) {
    setLoading(false);
    banner(e.http == 404 ? "Entry gone" : errorText(e, false));
    post(EvtType::EntryUpdated, "", 0, false);
    return;
  }
  lockedFeed().with([&](FeedState& s) { s.store.openEntry(std::move(entry)); });
  post(EvtType::EntryUpdated);

  std::vector<Reply> replies;
  std::string next;
  e = fetchList(kReplyLimit, [&](int limit) {
    replies.clear();
    return api_.replies(c.a, "", limit, replies, next);
  });
  if (e.ok())
    lockedFeed().with([&](FeedState& s) { s.store.appendReplies(std::move(replies), next); });
  else fail(e);
  post(EvtType::RepliesLoaded);

  bool watching = false;
  if (api_.watchStatus(c.a, watching).ok())
    lockedFeed().with([&](FeedState& s) { s.store.setWatching(watching); });
  setLoading(false);
  post(EvtType::EntryUpdated);
}

// Replies after the held ones: from the cursor, else from the newest held.
void FeedWorker::fetchNewReplies(const std::string& postId) {
  if (postId.empty()) return;
  std::string cursor =
      lockedFeed().with([](FeedState& s) { return s.store.repliesResumeCursor(); });
  std::vector<Reply> replies;
  std::string next;
  ApiError e = fetchList(kReplyLimit, [&](int limit) {
    replies.clear();
    return api_.replies(postId, cursor, limit, replies, next);
  });
  if (e.ok()) {
    lockedFeed().with([&](FeedState& s) {
      const Entry* open = s.store.entry();
      if (open && open->id == postId) s.store.appendReplies(std::move(replies), next);
    });
  } else {
    fail(e);
  }
  post(EvtType::RepliesLoaded);
}

void FeedWorker::moreReplies() { fetchNewReplies(openPostId()); }

// ---- writes (each from a key press, never retried) ----

void FeedWorker::postEntry(const Command& c) {
  std::string postId;
  ApiError e = api_.postEntry(c.a, postId);
  if (!e.ok()) {
    post(EvtType::EntryPosted, errorText(e, false), e.http, false);
    return;
  }
  post(EvtType::EntryPosted, postId);
}

void FeedWorker::postReply(const Command& c) {
  std::string replyId;
  ApiError e = api_.postReply(c.a, replyId);
  if (!e.ok()) {
    post(EvtType::FeedWriteDone, errorText(e, false), e.http, false);
    return;
  }
  post(EvtType::FeedWriteDone);
  fetchNewReplies(openPostId());
}

void FeedWorker::remove(const Command& c, bool reply) {
  ApiError e = reply ? api_.deleteReply(c.a) : api_.deleteEntry(c.a);
  if (!e.ok()) {
    banner(errorText(e, true));
    return;
  }
  lockedFeed().with([&](FeedState& s) { s.store.markDeleted(c.a); });
  if (reply) {
    post(EvtType::RepliesLoaded);
  } else {
    post(EvtType::FeedUpdated);
    post(EvtType::EntryUpdated);
    post(EvtType::SideListUpdated);
  }
}

void FeedWorker::flag(const Command& c, bool reply) {
  ApiError e = reply ? api_.flagReply(c.a, c.b) : api_.flagEntry(c.a, c.b);
  if (e.ok()) banner("Reported", true);
  else banner(errorText(e, true));
}

void FeedWorker::bookmark(const Command& c) {
  std::string bookmarkId;
  ApiError e = api_.addBookmark(c.a, c.b, bookmarkId);
  if (!e.ok()) {
    banner(errorText(e, false));
    return;
  }
  // Saved, but the reply carried no id: reload the map rather than invite a
  // duplicate press.
  if (bookmarkId.empty()) loadBookmarkMap();
  else lockedFeed().with([&](FeedState& s) { s.store.addBookmark(Bookmark{bookmarkId, c.a, c.b}); });
  post(EvtType::EntryUpdated);
  post(EvtType::FeedUpdated);
}

void FeedWorker::unbookmark(const Command& c) {
  std::string bookmarkId =
      lockedFeed().with([&](FeedState& s) { return s.store.bookmarkIdFor(c.a); });
  if (bookmarkId.empty()) {
    banner("Bookmarks not loaded yet");
    loadBookmarkMap();
    post(EvtType::EntryUpdated);
    return;
  }
  ApiError e = api_.removeBookmark(bookmarkId);
  if (!e.ok()) {
    banner(errorText(e, false));
    return;
  }
  lockedFeed().with([&](FeedState& s) { s.store.removeBookmark(c.a); });
  post(EvtType::EntryUpdated);
  post(EvtType::FeedUpdated);
}

void FeedWorker::watch(const Command& c, bool on) {
  ApiError e = api_.setWatch(c.a, on);
  if (!e.ok()) {
    banner(errorText(e, false));
    return;
  }
  lockedFeed().with([&](FeedState& s) {
    const Entry* open = s.store.entry();
    if (open && open->id == c.a) s.store.setWatching(on);
  });
  post(EvtType::EntryUpdated);
}

// ---- Watched and Bookmarks lists ----

void FeedWorker::openWatched(bool append) {
  std::string cursor;
  if (append) {
    cursor = lockedFeed().with([](FeedState& s) { return s.store.sideCursor(); });
    if (cursor.empty()) {
      post(EvtType::SideListUpdated);
      return;
    }
  }
  std::vector<std::string> postIds;
  std::string next;
  setLoading(true);
  ApiError e = fetchList(kSideLimit, [&](int limit) {
    postIds.clear();
    return api_.watches(cursor, postIds, next, limit);
  });
  if (!e.ok()) {
    setLoading(false);
    fail(e);
    post(EvtType::SideListUpdated, "", 0, false);
    return;
  }
  resolveSide(postIds, next, append);
}

void FeedWorker::openBookmarks(bool append) {
  std::string cursor;
  if (append) {
    cursor = lockedFeed().with([](FeedState& s) { return s.store.sideCursor(); });
    if (cursor.empty()) {
      post(EvtType::SideListUpdated);
      return;
    }
  }
  std::vector<Bookmark> page;
  std::string next;
  setLoading(true);
  ApiError e = fetchList(kSideLimit, [&](int limit) {
    page.clear();
    return api_.bookmarks(cursor, page, next, limit);
  });
  std::vector<std::string> postIds, replyIds;
  if (e.ok()) {
    for (const auto& b : page) {
      std::string postId = b.postId;
      if (postId.empty() && !b.replyId.empty()) {
        // A reply bookmark: the reply names its entry.
        Reply r;
        ApiError re = api_.reply(b.replyId, r);
        if (re.http == 404) continue;
        if (!re.ok()) {
          e = re;
          break;
        }
        postId = r.postId;
      }
      if (postId.empty()) continue;
      postIds.push_back(postId);
      replyIds.push_back(b.replyId);
    }
  }
  if (!e.ok()) {
    setLoading(false);
    fail(e);
    post(EvtType::SideListUpdated, "", 0, false);
    return;
  }
  lockedFeed().with([&](FeedState& s) {
    for (const auto& b : page) s.store.addBookmark(b);
  });
  resolveSide(postIds, next, append, replyIds);
}

// Fetches each listed entry (one read each), dedupes by postId and stores the
// page. A 404 skips that entry; any other error drops the whole page.
bool FeedWorker::resolveSide(const std::vector<std::string>& postIds, const std::string& next,
                             bool append, const std::vector<std::string>& replyIds) {
  std::set<std::string> held;
  if (append) {
    lockedFeed().with([&](FeedState& s) {
      for (const auto& e : s.store.sideList()) held.insert(e.id);
    });
  }
  std::vector<Entry> entries;
  std::vector<std::string> entryReplyIds;
  for (size_t i = 0; i < postIds.size(); ++i) {
    const std::string& id = postIds[i];
    if (!held.insert(id).second) continue;
    Entry entry;
    ApiError e = api_.entry(id, entry);
    if (e.http == 404) continue;
    if (!e.ok()) {
      setLoading(false);
      fail(e);
      post(EvtType::SideListUpdated, "", 0, false);
      return false;
    }
    entries.push_back(std::move(entry));
    entryReplyIds.push_back(i < replyIds.size() ? replyIds[i] : std::string());
  }
  lockedFeed().with([&](FeedState& s) {
    s.store.setSideList(std::move(entries), next, append, std::move(entryReplyIds));
    s.loading = false;
  });
  post(EvtType::SideListUpdated);
  return true;
}

// ---- notifications ----

void FeedWorker::onNotice(const Mention& m, bool buzzer) {
  if (!seen_.insert(m.id).second) return;
  if (seen_.size() > kMaxSeen) {
    seen_.clear();
    seen_.insert(m.id);
  }
  if (m.type == "post_cooldown") {
    post(EvtType::FeedAlert, "Entry held for review (saved as a note)", 0);
    return;
  }
  // A notice without a target would open GET /v1/posts/; drop it.
  if (m.targetId.empty()) return;
  if (m.targetId == openPostId()) {
    // The entry is on screen: refresh its replies instead of alerting.
    api_.markNotificationRead(m.id);
    fetchNewReplies(m.targetId);
    return;
  }
  std::string preview = m.preview;
  truncateCodePoints(preview, kPreviewCodePoints);
  bool mention = endsWith(m.type, "_mention");
  std::string text =
      "@" + m.actorUsername + (mention ? " mentioned you: " : " replied: ") + preview;
  lockedFeed().with([&](FeedState& s) {
    for (const auto& n : s.notices)
      if (n.id == m.id) return;
    s.notices.push_back(FeedNotice{m.id, m.targetId, text});
    if (s.notices.size() > kMaxNotices) s.notices.erase(s.notices.begin());
  });
  post(EvtType::FeedAlert, text, buzzer ? 1 : 0);
}

}  // namespace rc

