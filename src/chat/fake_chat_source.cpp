#if RC_FAKE_API

#include <Arduino.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <cctype>
#include <map>
#include <mutex>
#include <vector>

#include "app/queues.h"
#include "chat/chat_source.h"
#include "cs/rate_limiter.h"
#include "store/locked_chat.h"

// The scripted fake API for the m9_fake build: no network, no account.
// Fixture rooms, a scripted message every 4 s, 12 people (3 idle), sends that
// echo after 300 ms, a 400 for "/dice x" and the real local send limit.
// C-Mail: rain (2 unread), moth and kiwi (empty). A DM send echoes, then the
// partner types for 2 s and replies; rain writes every 45 s; starting a
// conversation with "nobody" fails with "User not found".

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
    out.push_back(msg("h" + std::to_string(page) + "-" + std::to_string(i), who, text, ts));
  }
  return out;
}

struct Echo {
  int64_t due;
  std::string slug, text;
};

enum class DmStep { Echo, TypingOn, Reply };

struct DmEvent {
  int64_t due;
  DmStep step;
  std::string conv, text;
};

std::string lower(std::string v) {
  for (char& ch : v) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return v;
}

const char* kReplies[] = {
    "ha, yes",
    "typing this from my phone, one sec",
    "same here honestly",
    "that's the best thing i've read all day",
    "ok, talk later",
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
  void seedCmail(int64_t now);
  void sendDm(const Command& c);
  void startConversation(const std::string& name);
  void appendDm(const std::string& conv, CircMessage m, bool alert);
  void setTyping(const std::string& conv, bool on);
  void runDm(int64_t now);

  // Lock order: mu_, then lockedChat(); never the other way round.
  std::mutex mu_;  // guards echoes_, sendL_, paused_, olderPages_, dms_, dmEvents_
  std::vector<Echo> echoes_;
  RateLimiter sendL_ = makeCircSendLimiter();
  RateLimiter cmailL_ = makeCmailSendLimiter();
  std::map<std::string, std::vector<CircMessage>> dms_;  // conversation id → messages
  std::vector<DmEvent> dmEvents_;
  int64_t nextRainDm_ = 0;
  int replyN_ = 0;
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
  seedCmail(now);
  postEvent(Event{EvtType::AuthState, kSelf, 1});
  postEvent(Event{EvtType::ConnState, "", 2});
  postEvent(Event{EvtType::RoomsUpdated});
  postEvent(Event{EvtType::ConversationsUpdated});
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
        s.dm = false;
        s.typing.clear();
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
        s.dm = false;
        s.store.clear();
        s.presence.clear();
        s.typing.clear();
      });
      postEvent(Event{EvtType::RoomChanged});
      break;
    case CmdType::Send: {
      bool dm = lockedChat().with([&](ChatState& s) { return s.dm && s.roomSlug == c.a; });
      if (dm) {
        sendDm(c);
        break;
      }
      std::lock_guard<std::mutex> g(mu_);
      int64_t now = mono();
      if (c.b.rfind("/dice x", 0) == 0) {
        postEvent(Event{EvtType::SendDone, "Usage: /dice [NdM], e.g. /dice 2d6", 400, false});
      } else if (c.b.rfind("/mute ", 0) == 0 || c.b.rfind("/unmute ", 0) == 0) {
        // Like the server: posts nothing, changes this room's mutes, replies.
        bool mute = c.b[1] == 'm';
        std::string name = c.b.substr(mute ? 6 : 8);
        for (char& ch : name) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        lockedChat().with([&](ChatState& s) {
          if (mute) s.muted[c.a].insert(name);
          else s.muted[c.a].erase(name);
        });
        postEvent(Event{EvtType::SendDone});
        postEvent(Event{EvtType::Banner, (mute ? "Muted " : "Unmuted ") + name});
        postEvent(Event{EvtType::RoomChanged});
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
      lockedChat().with([&](ChatState& s) {
        s.readAt[c.a] = s.store.newestTimestamp();
        for (auto& cv : s.conversations)
          if (cv.id == c.a) cv.unread = 0;
      });
      postEvent(Event{EvtType::ConversationsUpdated});
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
    case CmdType::OpenConversation: {
      std::vector<CircMessage> msgs;
      {
        std::lock_guard<std::mutex> g(mu_);
        msgs = dms_[c.a];
      }
      lockedChat().with([&](ChatState& s) {
        s.roomSlug = c.a;
        s.dm = true;
        s.noOlder = true;
        s.presence.clear();
        s.typing.clear();
        s.store.replaceAll(std::move(msgs));
        s.readAt[c.a] = s.store.newestTimestamp();
        for (auto& cv : s.conversations)
          if (cv.id == c.a) cv.unread = 0;
      });
      postEvent(Event{EvtType::RoomChanged});
      postEvent(Event{EvtType::ConversationsUpdated});
      break;
    }
    case CmdType::StartConversation:
      startConversation(c.a);
      break;
    case CmdType::RefreshConversations:
      postEvent(Event{EvtType::ConversationsUpdated});
      break;
    case CmdType::Activity:
    case CmdType::Typing:  // the fake has no one to show our typing to
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
    if (s.roomSlug.empty() || s.dm) return;
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
    runDm(now);
    if (!paused && now >= nextScript) {
      nextScript = now + 4000;
      scripted();
    }
  }
}

// ---- C-Mail ----

void FakeChatSource::seedCmail(int64_t now) {
  auto dm = [&](const std::string& who, const std::string& text, int64_t ts) {
    CircMessage m = msg("dm" + std::to_string(++seq_), who, text, ts);
    return m;
  };
  {
    std::lock_guard<std::mutex> g(mu_);
    dms_["c-rain"] = {
        dm(kSelf, "did you get the m9 running?", now - 50 * kMin),
        dm("rain", "yes! the keyboard took some getting used to", now - 12 * kMin),
        dm("rain", "want to try c-mail on it?", now - 11 * kMin),
    };
    dms_["c-moth"] = {
        dm("moth", "thanks for the link earlier", now - 26 * 60 * kMin),
        dm(kSelf, "any time", now - 25 * 60 * kMin),
    };
    dms_["c-kiwi"] = {};
    nextRainDm_ = mono() + 45000;
  }
  lockedChat().with([&](ChatState& s) {
    s.conversations = {
        ConversationInfo{"c-rain", "u-rain", "rain", "want to try c-mail on it?", now - 11 * kMin, 2,
                         false},
        ConversationInfo{"c-moth", "u-moth", "moth", "any time", now - 25 * 60 * kMin, 0, false},
        ConversationInfo{"c-kiwi", "u-kiwi", "kiwi", "", 0, 0, false},
    };
  });
}

void FakeChatSource::sendDm(const Command& c) {
  std::lock_guard<std::mutex> g(mu_);
  int64_t now = mono();
  if (!cmailL_.tryAcquire(now)) {
    int64_t s = (cmailL_.retryAfterMs(now) + 999) / 1000;
    postEvent(Event{EvtType::SendDone, "Slow down: try in " + std::to_string(s) + "s", 429, false});
    return;
  }
  dmEvents_.push_back(DmEvent{now + 300, DmStep::Echo, c.a, c.b});
  dmEvents_.push_back(DmEvent{now + 1300, DmStep::TypingOn, c.a, ""});
  dmEvents_.push_back(
      DmEvent{now + 3300, DmStep::Reply, c.a, kReplies[replyN_++ % 5]});
}

void FakeChatSource::startConversation(const std::string& name) {
  std::string want = lower(name);
  if (!want.empty() && want[0] == '@') want.erase(0, 1);
  if (want.empty() || want == "nobody") {
    postEvent(Event{EvtType::ConversationStarted, "User not found", 404, false});
    return;
  }
  if (want == kSelf) {
    postEvent(Event{EvtType::ConversationStarted, "You can't message yourself", 400, false});
    return;
  }
  std::string id = "c-" + want;
  {
    std::lock_guard<std::mutex> g(mu_);
    dms_[id];  // creates it empty if new
  }
  lockedChat().with([&](ChatState& s) {
    for (const auto& cv : s.conversations)
      if (cv.id == id) return;
    s.conversations.insert(s.conversations.begin(),
                           ConversationInfo{id, "u-" + want, want, "", 0, 0, false});
  });
  postEvent(Event{EvtType::ConversationsUpdated});
  postEvent(Event{EvtType::ConversationStarted, id});
}

// Stores a DM and shows it if its conversation is open; otherwise counts it
// unread and, for the partner's messages, raises an alert.
void FakeChatSource::appendDm(const std::string& conv, CircMessage m, bool alert) {
  {
    std::lock_guard<std::mutex> g(mu_);
    dms_[conv].push_back(m);
  }
  bool open = false;
  std::string who = m.username, text = m.content;
  lockedChat().with([&](ChatState& s) {
    open = s.dm && s.roomSlug == conv;
    for (auto& cv : s.conversations) {
      if (cv.id != conv) continue;
      cv.lastMessage = m.content;
      cv.lastMessageAt = m.timestamp;
      if (!open && m.username != kSelf) ++cv.unread;
    }
    if (open) {
      s.store.upsert(std::move(m));
      s.readAt[conv] = s.store.newestTimestamp();
    }
  });
  postEvent(Event{EvtType::ConversationsUpdated});
  if (open) postEvent(Event{EvtType::RoomChanged});
  else if (alert) postEvent(Event{EvtType::CmailAlert, "C-Mail from @" + who + ": " + text});
}

void FakeChatSource::setTyping(const std::string& conv, bool on) {
  bool changed = lockedChat().with([&](ChatState& s) {
    if (!s.dm || s.roomSlug != conv) return false;
    std::string partner;
    for (const auto& cv : s.conversations)
      if (cv.id == conv) partner = cv.partner;
    if (partner.empty()) return false;
    TypingEntry& e = s.typing["u-" + partner];
    e.userId = "u-" + partner;
    e.username = partner;
    e.typing = on;
    e.timestamp = epochMs();
    return true;
  });
  if (changed) postEvent(Event{EvtType::TypingChanged});
}

void FakeChatSource::runDm(int64_t now) {
  std::vector<DmEvent> due;
  bool rain = false;
  {
    std::lock_guard<std::mutex> g(mu_);
    for (auto it = dmEvents_.begin(); it != dmEvents_.end();) {
      if (it->due <= now) {
        due.push_back(*it);
        it = dmEvents_.erase(it);
      } else {
        ++it;
      }
    }
    if (now >= nextRainDm_) {
      nextRainDm_ = now + 45000;
      rain = true;
    }
  }
  for (auto& e : due) {
    std::string id = "dm" + std::to_string(++seq_);
    switch (e.step) {
      case DmStep::Echo:
        appendDm(e.conv, msg(id, kSelf, e.text, epochMs()), false);
        postEvent(Event{EvtType::SendDone});
        break;
      case DmStep::TypingOn:
        setTyping(e.conv, true);
        break;
      case DmStep::Reply: {
        setTyping(e.conv, false);
        std::string partner = e.conv.substr(2);  // "c-<name>"
        appendDm(e.conv, msg(id, partner, e.text, epochMs()), true);
        break;
      }
    }
  }
  if (rain) {
    static const char* lines[] = {"you around?", "look outside, it's pouring",
                                  "sent you a link in #music", "ping"};
    std::string id = "dm" + std::to_string(++seq_);
    appendDm("c-rain", msg(id, "rain", lines[seq_ % 4], epochMs()), true);
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
