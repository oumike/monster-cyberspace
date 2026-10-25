#if RC_FAKE_API

#include <Arduino.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <mutex>
#include <vector>

#include "app/queues.h"
#include "chat/chat_source.h"
#include "cs/rate_limiter.h"
#include "store/locked_chat.h"

// The scripted fake API for the m9_fake build: no network, no account.
// Fixture rooms, a scripted message every 4 s, 12 people (3 idle), sends that
// echo after 300 ms, a 400 for "/dice x" and the real local send limit.

namespace rc {

namespace {

constexpr const char* kSelf = "oumike";
constexpr int64_t kMin = 60000;

int64_t mono() { return esp_timer_get_time() / 1000; }

int64_t epochMs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  int64_t ms = int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
  // No SNTP in this build: start the clock at a fixed date if unset.
  return ms < 1700000000000LL ? 1782900000000LL + ms : ms;
}

const char* kNicks[] = {"tardibyte", "rain",   "genghis", "zed",   "moth", "kiwi",
                        "ferrous",   "lumen",  "quill",   "orbit", "sable"};

CircMessage msg(const std::string& id, const std::string& user, const std::string& text,
                int64_t ts) {
  CircMessage m;
  m.id = id;
  m.userId = "u-" + user;
  m.username = user;
  m.content = text;
  m.timestamp = ts;
  return m;
}

std::vector<CircMessage> history(const std::string& slug, int64_t endTs, int count, int page) {
  static const char* lines[] = {
      "anyone around?",
      "just got the m9 working, the keyboard is tiny but fine",
      "what's everyone listening to",
      "it's raining here again",
      "does anyone remember the old BBS door games",
      "brb, coffee",
      "the terminal font on this thing is crisp",
      "has anyone tried the new feed layout",
      "lol",
      "i keep typing ls in the chat box",
  };
  std::vector<CircMessage> out;
  for (int i = 0; i < count; ++i) {
    int64_t ts = endTs - int64_t(count - i) * 97000;
    const char* who = kNicks[(i * 7 + page * 3) % 11];
    std::string text = std::string(lines[(i + page) % 10]);
    if (slug != "general") text = "[" + slug + "] " + text;
    out.push_back(msg("h" + std::to_string(page) + "-" + std::to_string(i), who, text, ts));
  }
  return out;
}

struct Echo {
  int64_t due;
  std::string slug, text;
};

class FakeChatSource : public ChatSource {
 public:
  void command(const Command& c) override;
  void start();

 private:
  static void taskMain(void* self) { static_cast<FakeChatSource*>(self)->run(); }
  void run();
  void scripted();
  void fillPresence(ChatState& s);

  std::mutex mu_;  // guards echoes_, sendL_, paused_, olderPages_
  std::vector<Echo> echoes_;
  RateLimiter sendL_ = makeCircSendLimiter();
  bool paused_ = false;
  int olderPages_ = 2;
  int step_ = 0, seq_ = 0;
};

void FakeChatSource::fillPresence(ChatState& s) {
  s.presence.clear();
  int64_t now = epochMs();
  for (int i = 0; i < 12; ++i) {
    PresenceEntry e;
    e.username = i == 0 ? kSelf : kNicks[i - 1];
    e.userId = "u-" + e.username;
    e.online = true;
    e.lastSeen = now;
    // Three idle: last active 20 minutes ago.
    e.lastActivity = (i == 3 || i == 7 || i == 10) ? now - 20 * kMin : now - kMin;
    e.isChatAdmin = i == 2;
    s.presence[e.userId] = e;
  }
}

void FakeChatSource::start() {
  int64_t now = epochMs();
  lockedChat().with([&](ChatState& s) {
    s.self = kSelf;
    s.rooms = {
        RoomInfo{"r-general", "general", "general", now - kMin, 0, 12},
        RoomInfo{"r-linux", "linux", "linux", now - 30 * kMin, 1, 4},
        RoomInfo{"r-music", "music", "music", now - 3 * 60 * kMin, 2, 2},
    };
    s.readAt["music"] = now - 4 * 60 * kMin;  // linux shows as unread
    s.readAt["linux"] = now - 60 * kMin;
  });
  postEvent(Event{EvtType::AuthState, kSelf, 1});
  postEvent(Event{EvtType::ConnState, "", 2});
  postEvent(Event{EvtType::RoomsUpdated});
  xTaskCreatePinnedToCore(taskMain, "fake", 8192, this, 3, nullptr, 0);
}

void FakeChatSource::command(const Command& c) {
  switch (c.type) {
    case CmdType::OpenRoom: {
      {
        std::lock_guard<std::mutex> g(mu_);
        olderPages_ = 2;
      }
      lockedChat().with([&](ChatState& s) {
        s.roomSlug = c.a;
        s.noOlder = false;
        s.store.replaceAll(history(c.a, epochMs(), 30, 0));
        fillPresence(s);
        s.readAt[c.a] = s.store.newestTimestamp();
      });
      postEvent(Event{EvtType::RoomChanged});
      postEvent(Event{EvtType::PresenceUpdated});
      break;
    }
    case CmdType::CloseRoom:
      lockedChat().with([](ChatState& s) {
        s.roomSlug.clear();
        s.store.clear();
        s.presence.clear();
      });
      postEvent(Event{EvtType::RoomChanged});
      break;
    case CmdType::Send: {
      std::lock_guard<std::mutex> g(mu_);
      int64_t now = mono();
      if (c.b.rfind("/dice x", 0) == 0) {
        postEvent(Event{EvtType::SendDone, "Usage: /dice [NdM], e.g. /dice 2d6", 400, false});
      } else if (!sendL_.tryAcquire(now)) {
        int64_t s = (sendL_.retryAfterMs(now) + 999) / 1000;
        postEvent(Event{EvtType::SendDone, "Slow down: try in " + std::to_string(s) + "s", 429,
                        false});
      } else {
        echoes_.push_back(Echo{now + 300, c.a, c.b});
      }
      break;
    }
    case CmdType::LoadOlder: {
      int pages;
      {
        std::lock_guard<std::mutex> g(mu_);
        pages = olderPages_ > 0 ? olderPages_-- : 0;
      }
      int added = 0;
      lockedChat().with([&](ChatState& s) {
        if (pages > 0 && s.store.size()) {
          for (auto& m : history(s.roomSlug, s.store.oldestTimestamp(), 20, 3 - pages))
            s.store.upsert(std::move(m)), ++added;
        }
        if (pages <= 1) s.noOlder = true;
      });
      postEvent(Event{EvtType::OlderLoaded, "", added});
      postEvent(Event{EvtType::RoomChanged});
      break;
    }
    case CmdType::Delete:
      lockedChat().with([&](ChatState& s) {
        if (CircMessage* m = s.store.find(c.a)) {
          m->deleted = true;
          m->content = "[DELETED]";
        }
      });
      postEvent(Event{EvtType::RoomChanged});
      break;
    case CmdType::Flag:
      postEvent(Event{EvtType::Banner, "Reported"});
      break;
    case CmdType::MarkRead:
      lockedChat().with([&](ChatState& s) { s.readAt[c.a] = s.store.newestTimestamp(); });
      break;
    case CmdType::OpenPresence:
    case CmdType::ClosePresence:
      postEvent(Event{EvtType::PresenceUpdated});
      break;
    case CmdType::RefreshRooms:
      postEvent(Event{EvtType::RoomsUpdated});
      break;
    case CmdType::Login:
      postEvent(Event{EvtType::AuthState, kSelf, 1});
      break;
    case CmdType::Logout:
      postEvent(Event{EvtType::AuthState, "", 0});
      break;
    case CmdType::ResendVerification:
      postEvent(Event{EvtType::Banner, "Verification email sent"});
      break;
    case CmdType::ScreenOff: {
      std::lock_guard<std::mutex> g(mu_);
      paused_ = true;
      break;
    }
    case CmdType::ScreenOn: {
      std::lock_guard<std::mutex> g(mu_);
      paused_ = false;
      break;
    }
    case CmdType::Activity:
      break;
  }
}

void FakeChatSource::scripted() {
  int64_t now = epochMs();
  std::string id = "s" + std::to_string(++seq_);
  const char* who = kNicks[seq_ % 11];
  bool mention = false;
  std::string room;
  lockedChat().with([&](ChatState& s) {
    if (s.roomSlug.empty()) return;
    room = s.roomSlug;
    CircMessage m = msg(id, who, "", now);
    switch (step_ % 8) {
      case 0: m.content = "anyone else on a handheld right now?"; break;
      case 1: m.content = "waves at everyone"; m.isAction = true; break;
      case 2: m.content = "rolled 2d6: 4, 6 (10)"; m.isDice = true; break;
      case 3: m.content = "the ending of that show was wild"; m.styles = {"spoiler"}; break;
      case 4:  // a little cat, base64 ASCII art
        m.content = "IC9cXy9cCiggby5vICkKID4gXiA8";
        m.styles = {"art"};
        break;
      case 5: {  // a deletion patch on an earlier message
        const auto& all = s.store.messages();
        if (all.size() > 3) {
          CircMessage* d = s.store.find(all[all.size() - 3].id);
          if (d && !d->deleted) {
            d->deleted = true;
            d->content = "[DELETED]";
          }
        }
        return;
      }
      case 6:
        m.content = std::string("@") + kSelf + " are you there? check this out";
        mention = true;
        break;
      default: m.content = "https://example.invalid/cat.gif"; m.gifUrl = m.content; break;
    }
    s.store.upsert(std::move(m));
    for (auto& r : s.rooms)
      if (r.slug == s.roomSlug) r.lastMessageAt = now;
  });
  ++step_;
  postEvent(Event{EvtType::RoomChanged});
  if (mention)
    postEvent(Event{EvtType::MentionAlert,
                    std::string("@") + who + " in #" + room + ": @" + kSelf + " are you there?"});
}

void FakeChatSource::run() {
  int64_t nextScript = mono() + 4000;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));
    int64_t now = mono();
    std::vector<Echo> due;
    bool paused;
    {
      std::lock_guard<std::mutex> g(mu_);
      paused = paused_;
      for (auto it = echoes_.begin(); it != echoes_.end();) {
        if (it->due <= now) {
          due.push_back(*it);
          it = echoes_.erase(it);
        } else {
          ++it;
        }
      }
    }
    for (auto& e : due) {
      std::string id = "me" + std::to_string(++seq_);
      lockedChat().with([&](ChatState& s) {
        if (s.roomSlug != e.slug) return;
        CircMessage m = msg(id, kSelf, e.text, epochMs());
        if (e.text.rfind("/me ", 0) == 0) {
          m.content = e.text.substr(4);
          m.isAction = true;
        }
        s.store.upsert(std::move(m));
        s.readAt[e.slug] = s.store.newestTimestamp();
      });
      postEvent(Event{EvtType::SendDone});
      postEvent(Event{EvtType::RoomChanged});
    }
    if (!paused && now >= nextScript) {
      nextScript = now + 4000;
      scripted();
    }
  }
}

FakeChatSource& fake() {
  static FakeChatSource f;
  return f;
}

}  // namespace

ChatSource& chatSource() { return fake(); }

void startChatSource(const Settings&) { fake().start(); }

}  // namespace rc

#endif
