#if !RC_FAKE_API

#include "app/net_worker.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <set>

#include "app/feed_worker.h"
#include "app/people_worker.h"
#include "app/queues.h"
#include "cs/cs_api.h"
#include "cs/rtdb.h"
#include "net/https_client.h"
#include "net/sse_client.h"
#include "net/tls_alloc.h"
#include "net/wifi_mgr.h"
#include "store/locked_chat.h"
#include "store/locked_feed.h"
#include "web/web_server.h"

namespace rc::networker {

namespace {

constexpr int64_t kWatchMs = 60000;
#if RC_LOW_MEM
// No PSRAM (the X4): one TLS connection at a time, so the open room is polled
// on the requests connection instead of streamed (X4 spec §4.2).
constexpr bool kPollRoom = true;
#else
constexpr bool kPollRoom = false;
#endif
constexpr int64_t kRoomPollMs = 60000;
// Messages fetched when a room opens, and per poll. Without PSRAM a body and
// a TLS session barely fit together, so ten.
constexpr int kRoomPollLimit = kPollRoom ? 10 : 20;
constexpr int kHistoryLimit = kPollRoom ? 10 : 50;
constexpr int64_t kMaxBackoffMs = 30000;
constexpr int64_t kPausedBannerMs = 30000;

int64_t mono() { return esp_timer_get_time() / 1000; }

int64_t epochMs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

void banner(const std::string& text, bool ok = true) {
  postEvent(Event{EvtType::Banner, text, 0, ok});
}

std::atomic<int> gConn{-1}, gAuth{-1};

void post(EvtType t, int code = 0, const std::string& text = "", bool ok = true) {
  if (t == EvtType::ConnState) gConn = code;
  if (t == EvtType::AuthState && ok) gAuth = code;
  postEvent(Event{t, text, code, ok});
}

// ---- the device's Wi-Fi screen ----

std::string gJoining;  // the network a WifiJoin is trying

void wipeStr(std::string& s) { std::fill(s.begin(), s.end(), '\0'); }

void wifiCommand(Command& c) {
  rc::Settings s;
  settings::load(s);
  switch (c.type) {
    case CmdType::WifiScan:
      post(EvtType::WifiScanned, 0, rc::encodeWifiScan(wifimgr::scanDetailed()));
      break;
    case CmdType::WifiJoin: {
      // n = 1: b is the password to save; 0: join with the saved one.
      if (c.n == 1 && rc::saveWifiNet(s.wifi, c.a, c.b) == rc::WifiSave::Full) {
        post(EvtType::WifiState, -2, "Five networks saved already: forget one first", false);
        break;
      }
      if (c.n == 1) settings::saveWifi(s.wifi);
      gJoining = c.a;
      wifimgr::setNetworks(s.wifi, c.a);
      post(EvtType::WifiState, 0, c.a);
      break;
    }
    case CmdType::WifiForget:
      if (rc::forgetWifiNet(s.wifi, c.a)) settings::saveWifi(s.wifi);
      wifimgr::setNetworks(s.wifi, "");
      post(EvtType::WifiState, 2, c.a);
      break;
    default:
      break;
  }
  wipeStr(c.b);
  for (auto& w : s.wifi) wipeStr(w.password);
}

void pollJoin() {
  const int r = wifimgr::takeJoinResult();
  if (r == 0) return;
  if (r > 0) post(EvtType::WifiState, 1, gJoining);
  else post(EvtType::WifiState, -1, "Couldn't join " + gJoining + ": check the password", false);
}

// Web logins wait here for the worker to finish the Login command.
std::mutex loginMu;
std::condition_variable loginCv;
uint32_t loginTicket = 0, loginDone = 0;
LoginResult loginResult;

void finishLogin(const LoginResult& r) {
  std::lock_guard<std::mutex> g(loginMu);
  loginResult = r;
  loginDone = loginTicket;
  loginCv.notify_all();
}

class Worker {
 public:
  explicit Worker(const Settings& s) : api_(http_), feed_(api_), people_(api_), email_(s.email) {
    if (!s.refreshToken.empty()) api_.setRefreshToken(s.refreshToken);
  }

  void loop() {
    for (;;) {
      Command c;
      if (nextCommand(c, 200)) handle(c);
      tick();
    }
  }

 private:
  // ---- connectivity and auth ----

  bool online() const { return wifimgr::connected(); }

  void setConn(int code) {
    if (code == conn_) return;
    conn_ = code;
    post(EvtType::ConnState, code);
  }

  // Treats an auth failure that the refresh can't fix as a sign-out.
  void checkAuthError(const ApiError& e) {
    if (e.code == "EMAIL_NOT_VERIFIED") {
      if (auth_ != 2) post(EvtType::AuthState, auth_ = 2, self_);
      return;
    }
    // A 401 here already survived one refresh; a 400/401 from the refresh
    // itself means the saved refresh token is no longer accepted.
    const ApiError& r = api_.lastRefreshError();
    bool deadToken = e.http == 401 || (!r.ok() && (r.http == 400 || r.http == 401));
    if (deadToken) {
      closeRoom();
      feed_.signOut();
      people_.signOut();
      api_.signOut();
      self_.clear();
      ready_ = false;
      post(EvtType::AuthState, auth_ = 0, "Signed out: please log in again", false);
    }
  }

  void bootstrap() {
    if (!api_.signedIn() || !online()) return;
    if (!api_.ensureFresh(mono(), time(nullptr))) {
      checkAuthError(api_.lastRefreshError());
      nextBootstrap_ = mono() + 10000;
      return;
    }
    std::string name;
    ApiError e = api_.me(name);
    if (!e.ok()) {
      checkAuthError(e);
      nextBootstrap_ = mono() + (e.code == "EMAIL_NOT_VERIFIED" ? 60000 : 10000);
      return;
    }
    self_ = name;
    lockedChat().with([&](ChatState& s) { s.self = name; });
    ready_ = true;
    post(EvtType::AuthState, auth_ = 1, name);
#if RC_LOW_MEM
    // For the hardware check on boards without PSRAM: the heap left once signed in.
    Serial.printf("[mem] signed in: heap free %u largest %u\n",
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
#endif
    refreshRooms();
    loadMutes();
    refreshConversations();
    nextWatch_ = mono() + kWatchMs;
  }

  void loadMutes() {
    MutedUsers m;
    if (api_.mutedUsers(m).ok()) lockedChat().with([&](ChatState& s) { s.muted = std::move(m); });
  }

  static bool changesMutes(const std::string& text) {
    return text.rfind("/mute ", 0) == 0 || text.rfind("/unmute ", 0) == 0 ||
           text.rfind("/unmuteall", 0) == 0;
  }

  // ---- rooms ----

  // A room's REST/RTDB key (its id), or a conversation id as is.
  std::string roomKey(const std::string& slug) {
    if (dm_) return slug;
    auto it = roomIds_.find(slug);
    return it != roomIds_.end() ? it->second : slug;
  }

  void refreshRooms() {
    std::vector<RoomInfo> rooms;
    ApiError e = api_.rooms(rooms);
    if (!e.ok()) {
      checkAuthError(e);
      return;
    }
    for (auto& r : rooms) roomIds_[r.slug] = r.id;
    lockedChat().with([&](ChatState& s) { s.rooms = rooms; });
    post(EvtType::RoomsUpdated);
  }

  void stopStreams() {
    msgSse_.stop();
    presSse_.stop();
  }

  void closeRoom() {
    stopStreams();
    if (!dm_ && !room_.empty() && presenceSent_ && online()) api_.leave(roomKey(room_));
    presenceSent_ = false;
    room_.clear();
    dm_ = false;
    historyLoaded_ = false;
    lockedChat().with([](ChatState& s) {
      s.store.clear();
      s.presence.clear();
      s.typing.clear();
      s.roomSlug.clear();
      s.dm = false;
      s.noOlder = false;
    });
    post(EvtType::RoomChanged);
  }

  // Opens a cIRC room (by slug) or, with dm, a C-Mail conversation (by id).
  void openRoom(const std::string& slug, bool dm) {
    if (slug == room_ && dm == dm_ && (kPollRoom ? historyLoaded_ : msgSse_.running())) {
      post(EvtType::RoomChanged);
      return;
    }
    closeRoom();
    room_ = slug;
    dm_ = dm;
    lockedChat().with([&](ChatState& s) {
      s.roomSlug = slug;
      s.dm = dm;
    });
    backoffMs_ = 1000;
    reconnectAt_ = 0;
    downSince_ = 0;
    if (!ready_ || !online()) return;  // tick() opens it once possible
    loadHistory();
    if (historyLoaded_) {
      if (kPollRoom) {
        nextPoll_ = mono() + kRoomPollMs;
        setConn(2);
      } else {
        openMsgStream();
        if (wantsSecondStream()) openPresStream();
      }
      sendPresence();
      markRead(slug);
    }
  }

  // Poll mode: the newest page, merged into the store.
  void pollRoom() {
    nextPoll_ = mono() + kRoomPollMs;
    if (room_.empty() || !historyLoaded_) return;
    std::vector<CircMessage> msgs;
    int64_t cursor = -1;
    ApiError e = dm_ ? api_.cmailHistory(room_, 0, msgs, cursor, kRoomPollLimit)
                     : api_.history(roomKey(room_), 0, msgs, cursor, kRoomPollLimit);
    if (!e.ok()) {
      checkAuthError(e);
      setConn(1);
      return;
    }
    setConn(2);
    const bool changed = lockedChat().with([&](ChatState& s) {
      const size_t n = s.store.size();
      const int64_t newest = s.store.newestTimestamp();
      for (auto& m : msgs) s.store.upsert(std::move(m));
      return s.store.size() != n || s.store.newestTimestamp() != newest;
    });
    if (changed) post(EvtType::RoomChanged);
  }

  void loadHistory() {
    std::vector<CircMessage> msgs;
    int64_t cursor = -1;
    ApiError e = dm_ ? api_.cmailHistory(room_, 0, msgs, cursor, kHistoryLimit)
                     : api_.history(roomKey(room_), 0, msgs, cursor, kHistoryLimit);
    if (!e.ok()) {
      checkAuthError(e);
      banner(e.message, false);
      scheduleReconnect();
      return;
    }
    lockedChat().with([&](ChatState& s) {
      s.store.replaceAll(std::move(msgs));
      s.noOlder = cursor < 0;
    });
    historyLoaded_ = true;
    post(EvtType::RoomChanged);
  }

  void loadOlder() {
    if (room_.empty() || !historyLoaded_) return;
    int64_t oldest = lockedChat().with([](ChatState& s) { return s.store.oldestTimestamp(); });
    if (oldest < 0) return;
    std::vector<CircMessage> msgs;
    int64_t cursor = -1;
    ApiError e = dm_ ? api_.cmailHistory(room_, oldest, msgs, cursor, kHistoryLimit)
                     : api_.history(roomKey(room_), oldest, msgs, cursor, kHistoryLimit);
    if (!e.ok()) {
      checkAuthError(e);
      banner(e.message, false);
      post(EvtType::OlderLoaded, 0);
      return;
    }
    int added = int(msgs.size());
    lockedChat().with([&](ChatState& s) {
      for (auto& m : msgs) s.store.upsert(std::move(m));
      if (cursor < 0 || added == 0) s.noOlder = true;
    });
    post(EvtType::OlderLoaded, added);
  }

  // ---- streams ----

  void openMsgStream() {
    const Tokens& t = api_.tokens();
    if (t.rtdbUrl.empty() || t.idToken.empty()) return;
    int64_t newest = lockedChat().with([](ChatState& s) { return s.store.newestTimestamp(); });
    // dm_messages rejects a limitToLast above 100.
    RtdbQuery q{true, newest, newest < 0 ? 50 : (dm_ ? 100 : 1000)};
    std::string path = (dm_ ? "dm_messages/" : "chat_messages/") + roomKey(room_);
    std::string url = rtdbStreamUrl(t.rtdbUrl, path, t.idToken, q);
    msgDown_ = false;
    msgSse_.start(
        url,
        [this](const SseEvent& ev) {
          StreamResult r =
              lockedChat().with([&](ChatState& s) { return applyChatEvent(s.store, ev); });
          if (r == StreamResult::Changed) post(EvtType::RoomChanged);
          else if (r == StreamResult::AuthRevoked) authRevoked_ = true;
        },
        [this](bool live) {
          if (live) msgLive_ = true;
          else msgDown_ = true;
        });
  }

  void openPresStream() {
    const Tokens& t = api_.tokens();
    if (room_.empty() || t.rtdbUrl.empty() || t.idToken.empty()) return;
    if (dm_) {
      // In a conversation this slot carries the typing flags.
      std::string url = rtdbStreamUrl(t.rtdbUrl, "dm_presence/" + room_, t.idToken, {});
      presSse_.start(
          url,
          [this](const SseEvent& ev) {
            StreamResult r =
                lockedChat().with([&](ChatState& s) { return applyTypingEvent(s.typing, ev); });
            if (r == StreamResult::Changed) post(EvtType::TypingChanged);
            else if (r == StreamResult::AuthRevoked) authRevoked_ = true;
          },
          [](bool) {});
      return;
    }
    std::string url = rtdbStreamUrl(t.rtdbUrl, "chat_presence/" + roomKey(room_), t.idToken, {});
    presSse_.start(
        url,
        [this](const SseEvent& ev) {
          StreamResult r =
              lockedChat().with([&](ChatState& s) { return applyPresenceEvent(s.presence, ev); });
          if (r == StreamResult::Changed) post(EvtType::PresenceUpdated);
          else if (r == StreamResult::AuthRevoked) authRevoked_ = true;
        },
        [](bool) {});
  }

  void seedPresence() {
    std::vector<PresenceEntry> users;
    ApiError e = api_.users(roomKey(room_), users);
    if (!e.ok()) return;
    lockedChat().with([&](ChatState& s) {
      s.presence.clear();
      for (auto& u : users) s.presence[u.userId] = u;
    });
    post(EvtType::PresenceUpdated);
  }

  void scheduleReconnect() {
    int64_t now = mono();
    if (!downSince_) downSince_ = now;
    reconnectAt_ = now + backoffMs_;
    backoffMs_ = std::min(backoffMs_ * 2, kMaxBackoffMs);
    setConn(online() ? 1 : 0);
  }

  void reconnect() {
    if (authRevoked_.exchange(false)) api_.expireNow();
    if (!api_.ensureFresh(mono(), time(nullptr))) {
      scheduleReconnect();
      return;
    }
    if (!historyLoaded_) {
      loadHistory();
      if (!historyLoaded_) return;
      markRead(room_);
    }
    if (kPollRoom) {
      backoffMs_ = 1000;
      downSince_ = 0;
      nextPoll_ = mono() + kRoomPollMs;
      setConn(2);
      if (!dm_ && !presenceSent_) sendPresence();
      return;
    }
    if (!msgSse_.running()) openMsgStream();
    if (wantsSecondStream() && !presSse_.running()) openPresStream();
    if (!dm_ && !presenceSent_) sendPresence();
  }

  // Presence in a room with the drawer open; typing flags in a conversation.
  bool wantsSecondStream() const { return dm_ || presenceWanted_; }

  // ---- presence and read state ----

  void sendPresence() {
    if (dm_ || room_.empty() || !screenOn_) return;
    PresenceCadence cad;
    ApiError e = api_.presence(roomKey(room_), lastActivity_ ? lastActivity_ : epochMs(), cad);
    if (e.ok()) {
      presenceSent_ = true;
      lockedChat().with([&](ChatState& s) { s.cadence = cad; });
      heartbeatMs_ = std::max<int64_t>(cad.heartbeatMs, 10000);
    }
    nextHeartbeat_ = mono() + heartbeatMs_;
  }

  void markRead(const std::string& slug) {
    if (slug.empty()) return;
    if (dm_) {
      if (slug != room_) return;
      if (!api_.cmailRead(slug).ok()) return;
      lockedChat().with([&](ChatState& s) {
        s.readAt[slug] = s.store.newestTimestamp();
        for (auto& c : s.conversations)
          if (c.id == slug) c.unread = 0;
      });
      post(EvtType::ConversationsUpdated);
      return;
    }
    ApiError e = api_.markRead(roomKey(slug));
    if (!e.ok()) return;
    lockedChat().with([&](ChatState& s) {
      int64_t at = s.store.newestTimestamp();
      for (auto& r : s.rooms)
        if (r.slug == slug) at = std::max(at, r.lastMessageAt);
      s.readAt[slug] = at;
    });
    // The open room is read to the bottom: its mentions are read too.
    auto range = pendingMentions_.equal_range(slug);
    for (auto it = range.first; it != range.second; ++it) api_.markNotificationRead(it->second);
    pendingMentions_.erase(range.first, range.second);
  }

  // Unread's Mark all read, from one key press: every notification (mentions
  // and Feed notices) in one call, then each unread conversation. Not retried.
  void markAllRead() {
    ApiError e = api_.markAllNotificationsRead();
    if (e.ok()) {
      pendingMentions_.clear();
      lockedFeed().with([](FeedState& s) { s.notices.clear(); });
    } else {
      checkAuthError(e);
    }
    std::vector<std::string> ids;
    lockedChat().with([&](ChatState& s) {
      for (const auto& c : s.conversations)
        if (c.unread > 0) ids.push_back(c.id);
    });
    bool mailOk = true;
    for (const auto& id : ids) {
      if (!api_.cmailRead(id).ok()) {
        mailOk = false;
        continue;
      }
      lockedChat().with([&](ChatState& s) {
        for (auto& c : s.conversations)
          if (c.id == id) c.unread = 0;
      });
    }
    if (!ids.empty()) post(EvtType::ConversationsUpdated);
    const bool ok = e.ok() && mailOk;
    post(EvtType::AllRead, 0,
         ok ? std::string("Marked all read") : e.ok() ? std::string("Some C-Mail stayed unread") : e.message,
         e.ok());
  }

  // ---- C-Mail ----

  // Refreshes the list and raises one alert per conversation each time a new
  // unread message arrives (never for the open conversation).
  void refreshConversations() {
    std::vector<ConversationInfo> list;
    ApiError e = api_.conversations(list);
    if (!e.ok()) {
      checkAuthError(e);
      return;
    }
    for (auto& c : list) {
      int64_t& seen = alertedAt_[c.id];
      bool open = dm_ && c.id == room_;
      if (c.unread > 0 && c.lastMessageAt > seen && !open) {
        std::string preview = c.lastMessage.size() > 80 ? c.lastMessage.substr(0, 80) : c.lastMessage;
        post(EvtType::CmailAlert, 0, "C-Mail from @" + c.partner + ": " + preview);
      }
      seen = std::max(seen, c.lastMessageAt);
    }
    lockedChat().with([&](ChatState& s) { s.conversations = std::move(list); });
    post(EvtType::ConversationsUpdated);
  }

  void startConversation(const std::string& username) {
    ConversationInfo c;
    ApiError e = api_.startConversation(username, c);
    if (!e.ok()) {
      checkAuthError(e);
      post(EvtType::ConversationStarted, e.http,
           e.http == 404 ? "No user named " + username : e.message, false);
      return;
    }
    lockedChat().with([&](ChatState& s) {
      for (const auto& x : s.conversations)
        if (x.id == c.id) return;
      s.conversations.insert(s.conversations.begin(), c);
    });
    post(EvtType::ConversationsUpdated);
    post(EvtType::ConversationStarted, 0, c.id);
  }

  void typing(const Command& c) {
    if (!dm_ || c.a != room_ || !screenOn_) return;
    if (c.n) {
      TypingCadence cad;
      if (api_.cmailTyping(room_, cad).ok())
        lockedChat().with([&](ChatState& s) { s.typingCadence = cad; });
    } else {
      api_.cmailTypingClear(room_);
    }
  }

  void logMemory() {
    size_t msgs = 0, pres = 0, convs = 0, feed = 0, replies = 0;
    lockedChat().with([&](ChatState& s) {
      msgs = s.store.messages().size();
      pres = s.presence.size();
      convs = s.conversations.size();
    });
    lockedFeed().with([&](FeedState& s) {
      feed = s.store.feed().size();
      replies = s.store.replies().size();
    });
    Serial.printf(
        "[mem] int %u blk %u min %u psram %u | msgs %u pres %u convs %u seen %u feed %u "
        "replies %u\n",
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), unsigned(msgs),
                  unsigned(pres), unsigned(convs), unsigned(seenMentions_.size()), unsigned(feed),
                  unsigned(replies));
#if RC_LOW_MEM
    // Stack headroom (bytes), for sizing the tasks on a board without PSRAM.
    TaskHandle_t ui = xTaskGetHandle("ui");
    Serial.printf("[mem] stack free: net %u ui %u\n", unsigned(uxTaskGetStackHighWaterMark(nullptr)),
                  ui ? unsigned(uxTaskGetStackHighWaterMark(ui)) : 0u);
#endif
  }

  void watch() {
    logMemory();
    refreshRooms();
    refreshConversations();
    std::vector<Mention> ms;
    ApiError e = api_.mentions(ms);
    if (!e.ok()) {
      checkAuthError(e);
      return;
    }
    for (auto& m : ms) {
      if (m.at <= 0) m.at = epochMs();
      if (!m.type.empty() && m.type != "chat_mention") {
        // The UI decides on the beep with its buzzer setting.
        feed_.onNotice(m, true);
        continue;
      }
      if (!seenMentions_.insert(m.id).second) continue;
      pendingMentions_.emplace(m.roomSlug, m.id);
      Event ev{EvtType::MentionAlert,
               "@" + m.actorUsername + " in #" + m.roomSlug + ": " + m.messageContent};
      ev.arg = m.roomSlug;
      ev.at = m.at;
      postEvent(std::move(ev));
    }
    if (seenMentions_.size() > 200) seenMentions_.clear();
  }

  // ---- commands ----

  bool needNet(const Command& c) {
    if (online() && ready_) return true;
    banner(online() ? "Not signed in" : "Offline", false);
    const char* why = online() ? "Not signed in" : "Offline";
    if (c.type == CmdType::Send) post(EvtType::SendDone, 0, why, false);
    if (c.type == CmdType::PostEntry) post(EvtType::EntryPosted, 0, why, false);
    if (c.type == CmdType::PostReply) post(EvtType::FeedWriteDone, 0, why, false);
    if (c.type == CmdType::StartConversation) post(EvtType::ConversationStarted, 0, why, false);
    if (c.type == CmdType::LoadOlder) post(EvtType::OlderLoaded, 0);
    if (c.type == CmdType::OpenEntry) post(EvtType::EntryUpdated, 0, "", false);
    // Settle the screens waiting on a page, or they stay "loading…".
    if (c.type == CmdType::MoreReplies) post(EvtType::RepliesLoaded);
    if (c.type == CmdType::OpenFeed || c.type == CmdType::FeedMore ||
        c.type == CmdType::FeedRefresh)
      post(EvtType::FeedUpdated);
    if (c.type == CmdType::OpenWatched || c.type == CmdType::OpenBookmarks ||
        c.type == CmdType::OpenGuild || c.type == CmdType::GuildThreadsMore)
      post(EvtType::SideListUpdated, 0, "", false);
    if (c.type == CmdType::OpenGuilds) post(EvtType::GuildsUpdated, 0, "", false);
    if (c.type == CmdType::OpenMembers) post(EvtType::MembersUpdated, 0, "", false);
    if (c.type == CmdType::MarkAllRead) post(EvtType::AllRead, 0, why, false);
    if (c.type == CmdType::PostGuildThread) post(EvtType::EntryPosted, 0, why, false);
    if (c.type == CmdType::JoinGuild || c.type == CmdType::LeaveGuild ||
        c.type == CmdType::PromoteGuild)
      post(EvtType::GuildWriteDone, 0, why, false);
    if (c.type == CmdType::OpenProfile) post(EvtType::ProfileUpdated, 0, "", false);
    if (c.type == CmdType::Follow || c.type == CmdType::Unfollow || c.type == CmdType::Poke)
      post(EvtType::PeopleWriteDone, 0, why, false);
    if (c.type == CmdType::OpenUserPosts) post(EvtType::SideListUpdated, 0, "", false);
    return false;
  }

  void handle(Command& c) {
    switch (c.type) {
      case CmdType::Activity:
        lastActivity_ = c.n;
        return;
      case CmdType::OpenRoom:
        openRoom(c.a, false);
        return;
      case CmdType::OpenConversation:
        openRoom(c.a, true);
        return;
      case CmdType::CloseRoom:
        closeRoom();
        return;
      case CmdType::ScreenOff:
        screenOn_ = false;
        stopStreams();
        if (!dm_ && !room_.empty() && presenceSent_ && online()) api_.leave(roomKey(room_));
        presenceSent_ = false;
        setConn(0);
        return;
      case CmdType::ScreenOn:
        screenOn_ = true;
        backoffMs_ = 1000;
        reconnectAt_ = 0;
        downSince_ = 0;
        nextPoll_ = 0;
        return;  // tick() reopens the streams (or polls)
      case CmdType::OpenPresence:
        presenceWanted_ = true;
        if (!dm_ && needNet(c) && !room_.empty()) {
          seedPresence();
          if (!kPollRoom && !presSse_.running()) openPresStream();
        }
        return;
      case CmdType::ClosePresence:
        presenceWanted_ = false;
        if (!dm_) presSse_.stop();
        return;
      case CmdType::Login:
        login(c);
        return;
      case CmdType::CloseFeed:
      case CmdType::CloseGuild:
        feed_.handle(c);
        return;
      case CmdType::OpenFeed:
        feed_.markOpen();
        break;
      case CmdType::SetupAp:
        wifimgr::setSetupAp(c.n != 0);
        return;
      case CmdType::WifiScan:
      case CmdType::WifiJoin:
      case CmdType::WifiForget:
        wifiCommand(c);
        return;
      case CmdType::Logout:
        closeRoom();
        feed_.signOut();
        people_.signOut();
        api_.signOut();
        self_.clear();
        ready_ = false;
        alertedAt_.clear();
        lockedChat().with([](ChatState& s) { s.conversations.clear(); });
        post(EvtType::AuthState, auth_ = 0);
        return;
      default:
        break;
    }
    if (!needNet(c)) return;
    if (FeedWorker::isFeedCommand(c.type)) {
      feed_.handle(c);
      return;
    }
    if (PeopleWorker::isPeopleCommand(c.type)) {
      people_.handle(c);
      return;
    }
    switch (c.type) {
      case CmdType::Send: {
        std::string id, reply;
        ApiError e = dm_ && c.a == room_ ? api_.cmailSend(c.a, c.b, id)
                                         : api_.send(roomKey(c.a), c.b, id, reply);
        if (e.ok()) {
          if (changesMutes(c.b)) loadMutes();
          post(EvtType::SendDone);
          if (kPollRoom && c.a == room_) pollRoom();  // show it without a stream
          if (!reply.empty()) banner(reply);
        } else {
          checkAuthError(e);
          post(EvtType::SendDone, e.http, e.message, false);
        }
        break;
      }
      case CmdType::LoadOlder:
        loadOlder();
        break;
      case CmdType::Delete: {
        if (dm_) break;  // C-Mail has no delete
        ApiError e = api_.deleteMessage(roomKey(room_), c.a);
        if (!e.ok()) banner(e.message, false);
        break;
      }
      case CmdType::Flag: {
        if (dm_) break;  // nor report
        ApiError e = api_.flag(roomKey(room_), c.a, c.b);
        banner(e.ok() ? "Reported" : e.message, e.ok());
        break;
      }
      case CmdType::MarkRead:
        markRead(c.a.empty() ? room_ : c.a);
        break;
      case CmdType::MarkAllRead:
        markAllRead();
        break;
      case CmdType::RefreshRooms:
        refreshRooms();
        break;
      case CmdType::RefreshConversations:
        refreshConversations();
        break;
      case CmdType::StartConversation:
        startConversation(c.a);
        break;
      case CmdType::Typing:
        typing(c);
        break;
      case CmdType::ResendVerification: {
        ApiError e = api_.resendVerification(api_.tokens().idToken);
        banner(e.ok() ? "Verification email sent" : e.message, e.ok());
        break;
      }
      default:
        break;
    }
  }

  void login(Command& c) {
    LoginResult r;
    if (!online()) {
      r.message = "Offline";
      post(EvtType::AuthState, auth_, r.message, false);
    } else {
      Tokens t;
      ApiError e = api_.login(c.a, c.b, t);
      if (!e.ok()) {
        r.message = e.message;
        post(EvtType::AuthState, auth_, e.message, false);
      } else {
        email_ = c.a;
        settings::saveAccount(c.a, t.refreshToken);
        ready_ = false;
        nextBootstrap_ = 0;
        bootstrap();
        r.ok = true;
        if (auth_ == 2) r.message = "Signed in; verify your email to chat";
        else if (auth_ != 1) r.message = "Signed in; finishing setup";
      }
    }
    std::fill(c.b.begin(), c.b.end(), '\0');
    r.code = auth_;
    finishLogin(r);
  }

  // ---- timers ----

  void tick() {
    wifimgr::reconnectWithBackoff();
    pollJoin();
    int64_t now = mono();

    if (!online()) {
      if (wasOnline_) {
        wasOnline_ = false;
        stopStreams();
        presenceSent_ = false;
      }
      setConn(0);
      return;
    }
    if (!wasOnline_) {
      wasOnline_ = true;
      backoffMs_ = 1000;
      reconnectAt_ = 0;
    }

    if (!ready_) {
      if (api_.signedIn() && now >= nextBootstrap_) bootstrap();
      if (!api_.signedIn() && auth_ != 0) post(EvtType::AuthState, auth_ = 0);
      if (!ready_) return;
    }

    feed_.tick(now, screenOn_);

    if (msgLive_.exchange(false)) {
      backoffMs_ = 1000;
      downSince_ = 0;
      pausedBanner_ = false;
      setConn(2);
    }

    if (kPollRoom && !room_.empty() && screenOn_) {
      if (!historyLoaded_ && now >= reconnectAt_) {
        reconnectAt_ = now + backoffMs_;
        reconnect();
      } else if (historyLoaded_ && now >= nextPoll_) {
        pollRoom();
      }
      if (!dm_ && now >= nextHeartbeat_) sendPresence();
    } else if (!room_.empty() && screenOn_) {
      if (msgDown_.exchange(false)) scheduleReconnect();
      bool needs = !historyLoaded_ || !msgSse_.running() ||
                   (wantsSecondStream() && !presSse_.running());
      if (needs && now >= reconnectAt_) {
        if (msgSse_.running() == false && reconnectAt_ == 0) setConn(1);
        reconnectAt_ = now + backoffMs_;  // until the stream reports live
        reconnect();
      }
      if (downSince_ && !pausedBanner_ && now - downSince_ > kPausedBannerMs) {
        pausedBanner_ = true;
        banner("Chat paused, reconnecting", false);
      }
      if (!dm_ && now >= nextHeartbeat_) sendPresence();
    }

    if (now >= nextWatch_) {
      nextWatch_ = now + kWatchMs;
      watch();
    }
  }

  HttpsClient http_;
  CsApi api_;
  FeedWorker feed_;
  PeopleWorker people_;
  SseClient msgSse_, presSse_;
  std::string email_, self_, room_;
  std::map<std::string, std::string> roomIds_;  // slug → id
  std::set<std::string> seenMentions_;
  std::multimap<std::string, std::string> pendingMentions_;  // slug → notification id
  std::map<std::string, int64_t> alertedAt_;  // conversation id → lastMessageAt alerted

  std::atomic<bool> msgLive_{false}, msgDown_{false}, authRevoked_{false};
  bool dm_ = false;  // room_ is a C-Mail conversation id
  bool ready_ = false, historyLoaded_ = false, presenceSent_ = false, presenceWanted_ = false;
  bool screenOn_ = true, wasOnline_ = false, pausedBanner_ = false;
  int conn_ = -1, auth_ = -1;
  int64_t lastActivity_ = 0, heartbeatMs_ = 30000, nextHeartbeat_ = 0, nextWatch_ = 0;
  int64_t nextBootstrap_ = 0, reconnectAt_ = 0, downSince_ = 0, backoffMs_ = 1000;
  int64_t nextPoll_ = 0;
};

void taskMain(void* arg) {
  static_cast<Worker*>(arg)->loop();
}

}  // namespace

int connState() { return gConn; }
int authState() { return gAuth; }

uint32_t beginLogin() {
  std::lock_guard<std::mutex> g(loginMu);
  return ++loginTicket;
}

bool waitLogin(uint32_t ticket, int timeoutMs, LoginResult& out) {
  std::unique_lock<std::mutex> l(loginMu);
  bool done = loginCv.wait_for(l, std::chrono::milliseconds(timeoutMs),
                               [&] { return loginDone >= ticket; });
  if (done) out = loginResult;
  return done;
}

void start(const Settings& s) {
  tls::usePsram();
  wifimgr::begin(s);
  web::begin();
  static Worker* w = new Worker(s);
  // One core (the X4): below the UI (2), or TLS handshakes and parsing hold
  // off key presses.
#if RC_LOW_MEM
  // Measured: about 4.4 KB used, TLS handshakes included.
  constexpr uint32_t kNetStack = 9216;
#else
  constexpr uint32_t kNetStack = 12288;
#endif
  xTaskCreatePinnedToCore(taskMain, "net", kNetStack, w, portNUM_PROCESSORS > 1 ? 3 : 1, nullptr, 0);
}

}  // namespace rc::networker

#endif
