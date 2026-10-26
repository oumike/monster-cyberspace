#if RC_FAKE_API

#include <ArduinoJson.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "app/queues.h"
#include "cs/rate_limiter.h"
#include "feed/compose.h"
#include "feed/feed_source.h"
#include "feed/markdown_text.h"
#include "store/locked_feed.h"

// The scripted fake Feed for the m9_fake build: no network, no account.
// 15 entries, newest first, served 10 to a page (Down at the end loads the
// last 5, then the list is complete):
//   1 oumike's own entry (deletable); a `reply` notice on it every 90 s
//   2 bookmarked, with 25 replies (two reply pages), some of them threaded
//   3 contentWarning "spiders"
//   4 two attachments ([img], [song])
//   5 NSFW only
//   6 watched
//   7 about 6 KB of Markdown: headings, a list, a link, an image, a code fence
//   8–15 everyday entries with 0–12 replies; oumike has replies on a few
// One reply on entry 2 is bookmarked too. The 90 s notice timer starts with
// the fake chat source at boot. Posts, replies, bookmarks, watches,
// deletes and flags apply locally under the real client limiters, with the
// same banners as CsApi. Deleting someone else's item gives "Not allowed".
// Commands are applied inline on the caller's task, as in fake_chat_source.

namespace rc {

namespace {

constexpr const char* kSelf = "oumike";
constexpr int64_t kMin = 60000;
constexpr int kFeedPage = 10, kReplyPage = 20, kSidePage = 10;
constexpr size_t kPreviewCodePoints = 80, kMaxNotices = 50;
constexpr uint64_t kNoticeEveryUs = 90ULL * 1000 * 1000;

int64_t mono() { return esp_timer_get_time() / 1000; }

int64_t epochMs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  int64_t ms = int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
  // No SNTP in this build: start the clock at a fixed date if unset.
  return ms < 1700000000000LL ? 1782900000000LL + ms : ms;
}

void post(EvtType t, const std::string& text = "", int code = 0, bool ok = true) {
  postEvent(Event{t, text, code, ok});
}

void banner(const std::string& text, bool ok = false) { post(EvtType::Banner, text, 0, ok); }

// CsApi's local limit message: "Slow down: try in Ns".
bool limited(RateLimiter& l, std::string& why) {
  int64_t now = mono();
  if (l.tryAcquire(now)) return false;
  int64_t s = (l.retryAfterMs(now) + 999) / 1000;
  if (s < 1) s = 1;
  why = "Slow down: try in " + std::to_string(s) + "s";
  return true;
}

const char* kNicks[] = {"tardibyte", "rain", "genghis", "zed",   "moth", "kiwi",
                        "ferrous",   "lumen", "quill",  "orbit", "sable"};

const char* kReplyLines[] = {
    "this is great, thanks for writing it up",
    "hard agree",
    "i had the same thing happen last week",
    "can you say more about the second part?",
    "bookmarking this for later",
    "ha! yes",
    "not sure i follow, but i like it",
    "the old ways were better, fight me",
    "did you try turning it off and on again",
    "reading this on a tiny screen and it still works",
};

std::string longMarkdown() {
  std::string md =
      "# Notes from a week on a handheld\n\n"
      "I spent a week reading Cyberspace on nothing but a **ThinkNode M9**. "
      "Here is what I learned, in no particular order.\n\n"
      "## The good\n\n"
      "- The keyboard is small but *accurate*.\n"
      "- Battery lasts a full day of chat.\n"
      "- Text-only is a feature, not a limit.\n\n"
      "More at [the project page](https://example.invalid/rhino).\n\n"
      "![a photo of the device on a desk](https://example.invalid/m9.jpg)\n\n"
      "## A snippet\n\n"
      "```\n"
      "for (;;) {\n"
      "  read();\n"
      "  think();\n"
      "  reply();\n"
      "}\n"
      "```\n\n"
      "> Small screens make you choose your words.\n\n";
  static const char* paras[] = {
      "The first day was mostly about the keys. Thumbs find the letters faster than "
      "you expect, and the trackball-style centre key becomes second nature. By the "
      "evening I was keeping up with #general without falling behind.",
      "Reading long entries is where the scroll matters. A line at a time is slow, a "
      "page at a time is fast, and the mix of both is just right for a feed like "
      "this one. Headings break the wall of text up nicely.",
      "I missed images less than I thought I would. A little [img] marker is enough "
      "to know something was there, and I could look later on a bigger screen if it "
      "mattered. Most of the time it did not.",
  };
  for (int part = 1; md.size() < 6000; ++part) {
    md += "## Part " + std::to_string(part) + "\n\n";
    for (const char* p : paras) {
      md += p;
      md += "\n\n";
    }
  }
  md += "That's all. Thanks for reading!\n";
  return md;
}

class FakeFeedSource : public FeedSource {
 public:
  FakeFeedSource();
  void command(const Command& c) override;

 private:
  static void onTimer(void* self) { static_cast<FakeFeedSource*>(self)->notice(); }
  void seed();
  void notice();

  // Each pulls from the script under mu_, then touches lockedFeed() once.
  std::vector<Entry> feedPage(const std::string& cursor, std::string& next);
  std::vector<Reply> replyPage(const std::string& postId, const std::string& cursor,
                               std::string& next);
  Entry* findEntry(const std::string& id);
  Reply* findReply(const std::string& id, std::string* postId = nullptr);
  std::string openPostId();

  void refreshFirst(bool reloadBookmarks);
  void more();
  void loadBookmarkMap();
  void openEntry(const Command& c);
  void fetchNewReplies(const std::string& postId);
  void postEntry(const Command& c);
  void postReply(const Command& c);
  void remove(const Command& c, bool reply);
  void flag();
  void bookmark(const Command& c);
  void unbookmark(const Command& c);
  void watch(const Command& c, bool on);
  void openSide(bool bookmarks, bool append);

  // Lock order: mu_, then lockedFeed(); never the other way round.
  std::mutex mu_;  // guards everything below
  std::vector<Entry> entries_;                    // newest first, full bodies
  std::map<std::string, std::vector<Reply>> replies_;  // postId → oldest first
  std::vector<Bookmark> bookmarks_;               // newest first
  std::vector<std::string> watched_;              // postIds, newest first
  RateLimiter entryL_ = makeEntryLimiter();
  RateLimiter replyL_ = makeReplyLimiter();
  RateLimiter deleteL_ = makeDeleteLimiter();
  RateLimiter flagL_ = makeFlagLimiter();
  RateLimiter bookmarkL_ = makeBookmarkLimiter();
  RateLimiter watchL_ = makeWatchLimiter();
  int seq_ = 0, noticeN_ = 0;
  esp_timer_handle_t timer_ = nullptr;
};

FakeFeedSource::FakeFeedSource() {
  seed();
  esp_timer_create_args_t args = {};
  args.callback = &FakeFeedSource::onTimer;
  args.arg = this;
  args.name = "fakefeed";
  if (esp_timer_create(&args, &timer_) == ESP_OK) esp_timer_start_periodic(timer_, kNoticeEveryUs);
}

void FakeFeedSource::seed() {
  struct Script {
    const char* author;
    const char* title;
    const char* body;
    int replies;
  };
  static const Script script[] = {
      {kSelf, "first post from the m9",
       "Typing this on a pocket terminal. Replies welcome, I'll see them come in.", 3},
      {"rain", "what are you reading this week?",
       "Drop a book, a zine, a long post, anything. I'm halfway through *The Dispossessed* "
       "and it is wonderful.",
       25},
      {"moth", "found one in the bath",
       "It was the size of a coin and it did not care that I was there. I named it Gerald.", 4},
      {"kiwi", "desk setup", "New desk, old keyboard, one plant. Photo and the song I had on.",
       5},
      {"zed", "", "late night thoughts, not for work. you know the drill.", 2},
      {"genghis", "watching this thread",
       "Has anyone got the old BBS door games running on modern hardware? Asking for "
       "everyone.",
       7},
      {"tardibyte", "", "", 6},  // the long Markdown entry
      {"ferrous", "rainy sunday", "Tea, a blanket, and the feed on a tiny screen.", 0},
      {"lumen", "", "Is it just me or is everything quieter today?", 2},
      {"quill", "a small poem", "screen glows green\nkeys click soft\nthe network hums", 12},
      {"orbit", "", "Launch window opens at dawn. Wish us luck.", 1},
      {"sable", "lost: one cat", "Black, answers to nothing, last seen judging me.", 4},
      {"rain", "", "Second post of the day, sorry, I have a lot of thoughts.", 0},
      {"moth", "soup", "Made too much soup. Come over.", 3},
      {"zed", "hello cyberspace", "First post! Be gentle.", 9},
  };
  int64_t now = epochMs();
  int n = 0;
  for (const Script& s : script) {
    ++n;
    Entry e;
    e.id = std::string("p") + (n < 10 ? "0" : "") + std::to_string(n);
    e.author = s.author;
    e.title = s.title;
    e.slug = e.id;
    e.body = s.body;
    e.createdMs = now - int64_t(n) * 37 * kMin;
    e.isPublic = n % 3 != 0;
    if (n == 3) e.contentWarning = "spiders";
    if (n == 5) e.nsfw = true;
    if (n == 4) e.attachments = {"[img]", "[song] Boards of Canada \xE2\x80\x93 Roygbiv"};
    if (n == 7) {
      e.title = "a week on a handheld";
      e.body = longMarkdown();
      e.topics = {"hardware", "longread"};
    }
    if (n == 2) e.topics = {"books"};
    std::vector<Reply>& rs = replies_[e.id];
    for (int i = 0; i < s.replies; ++i) {
      Reply r;
      r.id = e.id + "-r" + std::to_string(i + 1);
      r.postId = e.id;
      // oumike chimes in on a few entries, so Delete can be tried on a reply.
      bool mine = (i == 2 && n % 2 == 0) || (n == 10 && i == 5);
      r.author = mine ? kSelf : kNicks[(n * 3 + i * 5) % 11];
      if (r.author == e.author && !mine) r.author = kNicks[(n * 3 + i * 5 + 1) % 11];
      r.body = kReplyLines[(n + i) % 10];
      r.createdMs = e.createdMs + int64_t(i + 1) * 3 * kMin;
      // Every fourth reply answers an earlier one.
      if (i >= 2 && i % 4 == 3) {
        const Reply& parent = rs[i - 2];
        r.parentId = parent.id;
        r.parentAuthor = parent.author;
      }
      if (n == 2 && i == 0) r.attachments = {"[img]"};
      if (n == 2 && i == 10)
        r.body = "Long one, sorry. " + std::string(kReplyLines[0]) + ". " + kReplyLines[3] +
                 " " + kReplyLines[7] + ". " + kReplyLines[2] + ", and " + kReplyLines[9] + ".";
      rs.push_back(std::move(r));
    }
    e.replies = s.replies;
    entries_.push_back(std::move(e));
  }
  bookmarks_ = {Bookmark{"b-p02-r4", "", "p02-r4"}, Bookmark{"b-p02", "p02", ""}};
  watched_ = {"p06"};
}

Entry* FakeFeedSource::findEntry(const std::string& id) {
  for (auto& e : entries_)
    if (e.id == id) return &e;
  return nullptr;
}

Reply* FakeFeedSource::findReply(const std::string& id, std::string* postId) {
  for (auto& [pid, rs] : replies_) {
    for (auto& r : rs) {
      if (r.id != id) continue;
      if (postId) *postId = pid;
      return &r;
    }
  }
  return nullptr;
}

std::string FakeFeedSource::openPostId() {
  return lockedFeed().with([](FeedState& s) {
    const Entry* e = s.store.entry();
    return e ? e->id : std::string();
  });
}

// The cursor is the id of the last entry sent, so a new entry on top doesn't
// shift the next page.
std::vector<Entry> FakeFeedSource::feedPage(const std::string& cursor, std::string& next) {
  std::lock_guard<std::mutex> g(mu_);
  size_t i = 0;
  if (!cursor.empty()) {
    while (i < entries_.size() && entries_[i].id != cursor) ++i;
    ++i;
  }
  std::vector<Entry> out;
  for (; i < entries_.size() && out.size() < size_t(kFeedPage); ++i) out.push_back(entries_[i]);
  next = i < entries_.size() && !out.empty() ? out.back().id : "";
  return out;
}

// Replies after the cursor reply (oldest first); "" starts at the oldest.
std::vector<Reply> FakeFeedSource::replyPage(const std::string& postId, const std::string& cursor,
                                             std::string& next) {
  std::lock_guard<std::mutex> g(mu_);
  const std::vector<Reply>& rs = replies_[postId];
  size_t i = 0;
  if (!cursor.empty()) {
    while (i < rs.size() && rs[i].id != cursor) ++i;
    i = i < rs.size() ? i + 1 : rs.size();
  }
  std::vector<Reply> out;
  for (; i < rs.size() && out.size() < size_t(kReplyPage); ++i) out.push_back(rs[i]);
  next = i < rs.size() && !out.empty() ? out.back().id : "";
  return out;
}

void FakeFeedSource::command(const Command& c) {
  switch (c.type) {
    case CmdType::OpenFeed: {
      refreshFirst(false);
      bool loaded = lockedFeed().with([](FeedState& s) { return s.store.bookmarksLoaded(); });
      if (!loaded) loadBookmarkMap();
      break;
    }
    case CmdType::CloseFeed: break;  // the fake has no timed refresh to stop
    case CmdType::FeedMore: more(); break;
    case CmdType::FeedRefresh: refreshFirst(true); break;
    case CmdType::OpenEntry: openEntry(c); break;
    case CmdType::MoreReplies: fetchNewReplies(openPostId()); break;
    case CmdType::PostEntry: postEntry(c); break;
    case CmdType::PostReply: postReply(c); break;
    case CmdType::DeleteEntry: remove(c, false); break;
    case CmdType::DeleteReply: remove(c, true); break;
    case CmdType::FlagEntry:
    case CmdType::FlagReply: flag(); break;
    case CmdType::Bookmark: bookmark(c); break;
    case CmdType::Unbookmark: unbookmark(c); break;
    case CmdType::Watch: watch(c, true); break;
    case CmdType::Unwatch: watch(c, false); break;
    case CmdType::OpenWatched: openSide(false, c.n == 1); break;
    case CmdType::OpenBookmarks: openSide(true, c.n == 1); break;
    default: break;
  }
}

// ---- feed list ----

void FakeFeedSource::refreshFirst(bool reloadBookmarks) {
  std::string next;
  std::vector<Entry> page = feedPage("", next);
  lockedFeed().with([&](FeedState& s) {
    s.store.mergeFirstPage(std::move(page), next);
    s.loading = false;
  });
  post(EvtType::FeedUpdated);
  if (reloadBookmarks) loadBookmarkMap();
}

void FakeFeedSource::more() {
  std::string cursor = lockedFeed().with([](FeedState& s) { return s.store.feedCursor(); });
  if (!cursor.empty()) {
    std::string next;
    std::vector<Entry> page = feedPage(cursor, next);
    lockedFeed().with([&](FeedState& s) { s.store.appendPage(std::move(page), next); });
  }
  post(EvtType::FeedUpdated);
}

void FakeFeedSource::loadBookmarkMap() {
  std::vector<Bookmark> all;
  {
    std::lock_guard<std::mutex> g(mu_);
    all = bookmarks_;
  }
  lockedFeed().with([&](FeedState& s) { s.store.setBookmarks(all); });
  post(EvtType::FeedUpdated);
}

// ---- entry and replies ----

void FakeFeedSource::openEntry(const Command& c) {
  if (!c.b.empty()) {
    lockedFeed().with([&](FeedState& s) {
      auto& n = s.notices;
      n.erase(std::remove_if(n.begin(), n.end(), [&](const FeedNotice& x) { return x.id == c.b; }),
              n.end());
    });
  }
  Entry entry;
  bool found;
  {
    std::lock_guard<std::mutex> g(mu_);
    Entry* e = findEntry(c.a);
    found = e != nullptr;
    if (found) entry = *e;
  }
  if (!found) {
    banner("Entry gone");
    post(EvtType::EntryUpdated, "", 0, false);
    return;
  }
  lockedFeed().with([&](FeedState& s) { s.store.openEntry(std::move(entry)); });
  post(EvtType::EntryUpdated);

  std::string next;
  std::vector<Reply> page = replyPage(c.a, "", next);
  lockedFeed().with([&](FeedState& s) { s.store.appendReplies(std::move(page), next); });
  post(EvtType::RepliesLoaded);

  bool watching;
  {
    std::lock_guard<std::mutex> g(mu_);
    watching = std::find(watched_.begin(), watched_.end(), c.a) != watched_.end();
  }
  lockedFeed().with([&](FeedState& s) {
    s.store.setWatching(watching);
    s.loading = false;
  });
  post(EvtType::EntryUpdated);
}

// Replies after the held ones: from the cursor, else from the newest held.
void FakeFeedSource::fetchNewReplies(const std::string& postId) {
  if (postId.empty()) return;
  std::string cursor =
      lockedFeed().with([](FeedState& s) { return s.store.repliesResumeCursor(); });
  std::string next;
  std::vector<Reply> page = replyPage(postId, cursor, next);
  lockedFeed().with([&](FeedState& s) {
    const Entry* open = s.store.entry();
    if (open && open->id == postId) s.store.appendReplies(std::move(page), next);
  });
  post(EvtType::RepliesLoaded);
}

// ---- writes (each from a key press, never retried) ----

void FakeFeedSource::postEntry(const Command& c) {
  JsonDocument doc;
  EntryDraft d;
  if (!deserializeJson(doc, c.a)) {
    d.body = doc["content"] | "";
    d.title = doc["title"] | "";
    d.cw = doc["contentWarning"] | "";
    d.isPublic = doc["isPublic"] | false;
    d.nsfw = doc["isNSFW"] | false;
    for (JsonVariantConst t : doc["topics"].as<JsonArrayConst>()) {
      if (!d.topics.empty()) d.topics += ",";
      d.topics += t.as<const char*>() ? t.as<const char*>() : "";
    }
  }
  std::string why = validateDraft(d);
  if (!why.empty()) {
    post(EvtType::EntryPosted, why, 400, false);
    return;
  }
  std::string id;
  {
    std::lock_guard<std::mutex> g(mu_);
    if (limited(entryL_, why)) {
      post(EvtType::EntryPosted, why, 429, false);
      return;
    }
    Entry e;
    e.id = "n" + std::to_string(++seq_);
    e.slug = e.id;
    e.author = kSelf;
    e.title = d.title;
    e.body = d.body;
    e.contentWarning = d.cw;
    e.topics = splitTopics(d.topics);
    e.isPublic = d.isPublic;
    e.nsfw = d.nsfw;
    e.createdMs = epochMs();
    replies_[e.id];
    id = e.id;
    entries_.insert(entries_.begin(), std::move(e));
  }
  post(EvtType::EntryPosted, id);
}

void FakeFeedSource::postReply(const Command& c) {
  JsonDocument doc;
  std::string postId, body, parentId;
  if (!deserializeJson(doc, c.a)) {
    postId = doc["postId"] | "";
    body = doc["content"] | "";
    parentId = doc["parentReplyId"] | "";
  }
  EntryDraft d;
  d.body = body;
  std::string why = validateDraft(d);
  if (!why.empty()) {
    post(EvtType::FeedWriteDone, why, 400, false);
    return;
  }
  {
    std::lock_guard<std::mutex> g(mu_);
    if (limited(replyL_, why)) {
      post(EvtType::FeedWriteDone, why, 429, false);
      return;
    }
    Entry* e = findEntry(postId);
    if (!e || e->deleted) {
      post(EvtType::FeedWriteDone, "Entry gone", 404, false);
      return;
    }
    Reply r;
    r.id = postId + "-m" + std::to_string(++seq_);
    r.postId = postId;
    r.author = kSelf;
    r.body = body;
    r.createdMs = epochMs();
    if (!parentId.empty()) {
      if (Reply* p = findReply(parentId)) {
        r.parentId = parentId;
        r.parentAuthor = p->author;
      }
    }
    replies_[postId].push_back(std::move(r));
    ++e->replies;
  }
  post(EvtType::FeedWriteDone);
  fetchNewReplies(openPostId());
}

void FakeFeedSource::remove(const Command& c, bool reply) {
  std::string why;
  {
    std::lock_guard<std::mutex> g(mu_);
    if (limited(deleteL_, why)) {
      banner(why);
      return;
    }
    if (reply) {
      Reply* r = findReply(c.a);
      if (!r) why = "Not found";
      else if (r->author != kSelf) why = "Not allowed";
      else r->deleted = true;
    } else {
      Entry* e = findEntry(c.a);
      if (!e) why = "Not found";
      else if (e->author != kSelf) why = "Not allowed";
      else e->deleted = true;
    }
  }
  if (!why.empty()) {
    banner(why);
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

void FakeFeedSource::flag() {
  std::string why;
  {
    std::lock_guard<std::mutex> g(mu_);
    if (limited(flagL_, why)) {
      banner(why);
      return;
    }
  }
  banner("Reported", true);
}

void FakeFeedSource::bookmark(const Command& c) {
  Bookmark b;
  {
    std::lock_guard<std::mutex> g(mu_);
    std::string why;
    if (limited(bookmarkL_, why)) {
      banner(why);
      return;
    }
    const std::string& target = c.b.empty() ? c.a : c.b;
    for (const auto& x : bookmarks_) {
      if ((c.b.empty() ? x.postId : x.replyId) == target) b = x;
    }
    if (b.id.empty()) {
      b = c.b.empty() ? Bookmark{"b" + std::to_string(++seq_), c.a, ""}
                      : Bookmark{"b" + std::to_string(++seq_), "", c.b};
      bookmarks_.insert(bookmarks_.begin(), b);
    }
  }
  lockedFeed().with([&](FeedState& s) { s.store.addBookmark(b); });
  post(EvtType::EntryUpdated);
  post(EvtType::FeedUpdated);
}

void FakeFeedSource::unbookmark(const Command& c) {
  std::string bookmarkId =
      lockedFeed().with([&](FeedState& s) { return s.store.bookmarkIdFor(c.a); });
  if (bookmarkId.empty()) {
    banner("Bookmarks not loaded yet");
    loadBookmarkMap();
    post(EvtType::EntryUpdated);
    return;
  }
  {
    std::lock_guard<std::mutex> g(mu_);
    std::string why;
    if (limited(bookmarkL_, why)) {
      banner(why);
      return;
    }
    auto& v = bookmarks_;
    v.erase(std::remove_if(v.begin(), v.end(), [&](const Bookmark& b) { return b.id == bookmarkId; }),
            v.end());
  }
  lockedFeed().with([&](FeedState& s) { s.store.removeBookmark(c.a); });
  post(EvtType::EntryUpdated);
  post(EvtType::FeedUpdated);
}

void FakeFeedSource::watch(const Command& c, bool on) {
  {
    std::lock_guard<std::mutex> g(mu_);
    std::string why;
    if (limited(watchL_, why)) {
      banner(why);
      return;
    }
    auto& v = watched_;
    v.erase(std::remove(v.begin(), v.end(), c.a), v.end());
    if (on) v.insert(v.begin(), c.a);
  }
  lockedFeed().with([&](FeedState& s) {
    const Entry* open = s.store.entry();
    if (open && open->id == c.a) s.store.setWatching(on);
  });
  post(EvtType::EntryUpdated);
}

// ---- Watched and Bookmarks lists ----

// Pages of kSidePage items; the cursor is the index of the next item. Reply
// bookmarks resolve to their entry, and entries already listed are skipped.
void FakeFeedSource::openSide(bool bookmarks, bool append) {
  std::string cursor;
  std::set<std::string> held;
  if (append) {
    lockedFeed().with([&](FeedState& s) {
      cursor = s.store.sideCursor();
      for (const auto& e : s.store.sideList()) held.insert(e.id);
    });
    if (cursor.empty()) {
      post(EvtType::SideListUpdated);
      return;
    }
  }
  std::vector<Entry> entries;
  std::vector<std::string> replyIds;
  std::vector<Bookmark> marks;
  std::string next;
  {
    std::lock_guard<std::mutex> g(mu_);
    size_t start = cursor.empty() ? 0 : size_t(strtoul(cursor.c_str(), nullptr, 10));
    size_t total = bookmarks ? bookmarks_.size() : watched_.size();
    size_t end = std::min(total, start + kSidePage);
    for (size_t i = start; i < end; ++i) {
      std::string postId, replyId;
      if (bookmarks) {
        const Bookmark& b = bookmarks_[i];
        marks.push_back(b);
        postId = b.postId;
        replyId = b.replyId;
        if (postId.empty()) findReply(b.replyId, &postId);
      } else {
        postId = watched_[i];
      }
      if (postId.empty() || !held.insert(postId).second) continue;
      if (const Entry* e = findEntry(postId)) {
        entries.push_back(*e);
        replyIds.push_back(replyId);
      }
    }
    next = end < total ? std::to_string(end) : "";
  }
  lockedFeed().with([&](FeedState& s) {
    for (const auto& b : marks) s.store.addBookmark(b);
    s.store.setSideList(std::move(entries), next, append, std::move(replyIds));
    s.loading = false;
  });
  post(EvtType::SideListUpdated);
}

// ---- the 90 s reply notice on entry 1 ----

// Runs on the esp_timer task, never inside lockedFeed().with(): a new reply
// lands on entry 1, then either shows (entry 1 open) or raises an alert.
void FakeFeedSource::notice() {
  static const char* lines[] = {
      "welcome to the feed!",
      "the m9 looks so cute, how is the keyboard?",
      "replying from my own handheld",
      "nice. what font is that?",
  };
  const std::string postId = "p01";
  std::string who, text, noticeId;
  {
    std::lock_guard<std::mutex> g(mu_);
    Entry* e = findEntry(postId);
    if (!e || e->deleted) return;
    int k = noticeN_++;
    who = kNicks[(k * 4 + 1) % 11];
    text = lines[k % 4];
    Reply r;
    r.id = postId + "-n" + std::to_string(++seq_);
    r.postId = postId;
    r.author = who;
    r.body = text;
    r.createdMs = epochMs();
    replies_[postId].push_back(std::move(r));
    ++e->replies;
    noticeId = "fn" + std::to_string(seq_);
  }
  if (openPostId() == postId) {
    fetchNewReplies(postId);
    return;
  }
  std::string preview = text;
  truncateCodePoints(preview, kPreviewCodePoints);
  std::string alert = "@" + who + " replied: " + preview;
  lockedFeed().with([&](FeedState& s) {
    s.notices.push_back(FeedNotice{noticeId, postId, alert});
    if (s.notices.size() > kMaxNotices) s.notices.erase(s.notices.begin());
  });
  // The UI decides on the beep with its buzzer setting.
  post(EvtType::FeedAlert, alert, 1);
}

}  // namespace

FeedSource& feedSource() {
  static FakeFeedSource s;
  return s;
}

}  // namespace rc

#endif
