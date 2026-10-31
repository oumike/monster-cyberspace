#if !RC_FAKE_API

#include "app/people_worker.h"

#include <utility>
#include <vector>

#include "app/queues.h"
#include "feed/markdown_text.h"
#include "store/locked_feed.h"
#include "store/locked_people.h"

namespace rc {

namespace {

constexpr int kFollowLimit = 50, kFollowPages = 10, kPostsLimit = 10;

void post(EvtType t, const std::string& text = "", int code = 0, bool ok = true) {
  postEvent(Event{t, text, code, ok});
}

void banner(const std::string& text) { post(EvtType::Banner, text, 0, false); }

std::string errorText(const ApiError& e) {
  if (e.code == "LOCAL_RATE_LIMIT") return e.message;  // "Slow down: try in N"
  if (e.http == 429) return "Slow down";
  return e.message;
}

void done(const std::string& text, bool ok, int code = 0) {
  post(EvtType::PeopleWriteDone, text, code, ok);
}

void setFeedLoading(bool on) {
  lockedFeed().with([on](FeedState& s) { s.loading = on; });
}

}  // namespace

PeopleWorker::PeopleWorker(CsApi& api) : api_(api) {}

bool PeopleWorker::isPeopleCommand(CmdType t) {
  switch (t) {
    case CmdType::OpenProfile:
    case CmdType::Follow:
    case CmdType::Unfollow:
    case CmdType::Poke:
    case CmdType::OpenUserPosts:
      return true;
    default:
      return false;
  }
}

void PeopleWorker::handle(const Command& c) {
  switch (c.type) {
    case CmdType::OpenProfile: openProfile(c.a); break;
    case CmdType::Follow: follow(c); break;
    case CmdType::Unfollow: unfollow(c); break;
    case CmdType::Poke: poke(c); break;
    case CmdType::OpenUserPosts: openUserPosts(c.a, c.n == 1); break;
    default: break;
  }
}

void PeopleWorker::signOut() {
  lockedPeople().with([](PeopleState& s) { s = PeopleState{}; });
}

void PeopleWorker::openProfile(const std::string& username) {
  lockedPeople().with([&](PeopleState& s) {
    s.wanted = username;
    s.loaded = false;
    s.loading = true;
  });
  Profile p;
  ApiError e = api_.profile(username, p);
  if (!e.ok()) {
    lockedPeople().with([](PeopleState& s) { s.loading = false; });
    banner(e.http == 404 ? "No such user" : errorText(e));
    post(EvtType::ProfileUpdated, "", e.http == 404 ? 404 : 0, false);
    return;
  }
  // Blocks change on the website; read them again with each profile.
  std::string self;
  std::vector<std::string> blocked;
  bool haveBlocked = api_.me(self, &blocked).ok();
  bool needFollows = lockedPeople().with(
      [](PeopleState& s) { return !s.follows.loaded() || !s.follows.cursor().empty(); });
  if (needFollows) loadFollowing();
  lockedPeople().with([&](PeopleState& s) {
    if (haveBlocked) s.blocked = std::move(blocked);
    if (s.wanted == username) {
      s.profile = std::move(p);
      s.loaded = true;
    }
    s.loading = false;
  });
  post(EvtType::ProfileUpdated);
}

void PeopleWorker::loadFollowing() {
  std::string cursor = lockedPeople().with([](PeopleState& s) {
    return s.follows.loaded() ? s.follows.cursor() : std::string();
  });
  for (int i = 0; i < kFollowPages; ++i) {
    std::vector<Follow> page;
    std::string next;
    // A failure leaves the index as it was; the screen then can't tell and
    // offers Follow (a 409 says "already following").
    if (!api_.following(cursor, kFollowLimit, page, next).ok()) return;
    bool more = lockedPeople().with([&](PeopleState& s) {
      s.follows.addPage(page, next);
      return !s.follows.cursor().empty();
    });
    if (!more) return;
    cursor = next;
  }
}

void PeopleWorker::refreshProfile(const std::string& username) {
  Profile p;
  if (api_.profile(username, p).ok()) {
    lockedPeople().with([&](PeopleState& s) {
      if (s.wanted == username) s.profile = std::move(p);
    });
  }
  post(EvtType::ProfileUpdated);
}

void PeopleWorker::follow(const Command& c) {
  std::string followId;
  ApiError e = api_.follow(c.a, followId);
  if (!e.ok()) {
    std::string text;
    if (e.http == 409) text = "Already following @" + c.b;
    else if (e.http == 403) text = "Not allowed";
    else if (e.http == 404) text = "No such user";
    else text = errorText(e);
    done(text, false, e.http);
    return;
  }
  if (followId.empty()) {
    // Followed, but the reply had no id: reload the list to find it.
    lockedPeople().with([](PeopleState& s) { s.follows.clear(); });
    loadFollowing();
  } else {
    lockedPeople().with([&](PeopleState& s) { s.follows.set(c.a, followId); });
  }
  done("Following @" + c.b, true);
  refreshProfile(c.b);
}

void PeopleWorker::unfollow(const Command& c) {
  std::string followId =
      lockedPeople().with([&](PeopleState& s) { return s.follows.followIdFor(c.a); });
  if (followId.empty()) {
    done("Not following @" + c.b, false);
    return;
  }
  ApiError e = api_.unfollow(followId);
  if (!e.ok() && e.http != 404) {
    done(e.http == 403 ? std::string("Not allowed") : errorText(e), false, e.http);
    return;
  }
  // 404: the follow was already gone.
  lockedPeople().with([&](PeopleState& s) { s.follows.erase(c.a); });
  done("Unfollowed @" + c.b, true);
  refreshProfile(c.b);
}

void PeopleWorker::poke(const Command& c) {
  ApiError e = api_.poke(c.a);
  if (!e.ok()) {
    std::string text;
    if (e.http == 400) text = "Can't poke yourself";
    else if (e.http == 403) text = "Can't poke @" + c.a;
    else if (e.http == 404) text = "No such user";
    else text = errorText(e);
    done(text, false, e.http);
    return;
  }
  done("Poked @" + c.a, true);
}

void PeopleWorker::openUserPosts(const std::string& username, bool append) {
  std::string cursor;
  if (append) {
    cursor = lockedFeed().with([](FeedState& s) { return s.store.sideCursor(); });
    if (cursor.empty()) {
      post(EvtType::SideListUpdated);
      return;
    }
  } else {
    lockedFeed().with([](FeedState& s) { s.store.setSideList({}, "", false); });
  }
  setFeedLoading(true);
  std::vector<Entry> entries;
  std::string next;
  int limit = kPostsLimit;
  ApiError e;
  // TOO_LARGE: ask again with a smaller page (10 → 5 → 2), as the Feed does.
  for (;;) {
    entries.clear();
    e = api_.userPosts(username, cursor, limit, entries, next);
    if (e.code != "TOO_LARGE") break;
    limit = nextSmallerLimit(limit);
    if (limit <= 0) break;
  }
  if (!e.ok()) {
    setFeedLoading(false);
    if (e.code == "TOO_LARGE") banner("Too large to load");
    else banner(e.http == 404 ? "No such user" : errorText(e));
    post(EvtType::SideListUpdated, "", 0, false);
    return;
  }
  lockedFeed().with([&](FeedState& s) {
    s.store.setSideList(std::move(entries), next, append);
    s.loading = false;
  });
  post(EvtType::SideListUpdated);
}

}  // namespace rc

#endif
