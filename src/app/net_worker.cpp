#if !RC_FAKE_API

#include "app/net_worker.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <algorithm>
#include <atomic>
#include <ctime>
#include <map>
#include <set>

#include "app/queues.h"
#include "cs/cs_api.h"
#include "cs/rtdb.h"
#include "net/https_client.h"
#include "net/sse_client.h"
#include "net/wifi_mgr.h"
#include "store/locked_chat.h"

namespace rc::networker {

namespace {

constexpr int64_t kWatchMs = 60000;
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

void post(EvtType t, int code = 0, const std::string& text = "", bool ok = true) {
  postEvent(Event{t, text, code, ok});
}

class Worker {
 public:
  explicit Worker(const Settings& s) : api_(http_), email_(s.email) {
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
    refreshRooms();
    nextWatch_ = mono() + kWatchMs;
  }

  // ---- rooms ----

  std::string roomKey(const std::string& slug) {
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
    if (!room_.empty() && presenceSent_ && online()) api_.leave(roomKey(room_));
    presenceSent_ = false;
    room_.clear();
    historyLoaded_ = false;
    lockedChat().with([](ChatState& s) {
      s.store.clear();
      s.presence.clear();
      s.roomSlug.clear();
      s.noOlder = false;
    });
    post(EvtType::RoomChanged);
  }

  void openRoom(const std::string& slug) {
    if (slug == room_ && msgSse_.running()) {
      post(EvtType::RoomChanged);
      return;
    }
    closeRoom();
    room_ = slug;
    lockedChat().with([&](ChatState& s) { s.roomSlug = slug; });
    backoffMs_ = 1000;
    reconnectAt_ = 0;
    downSince_ = 0;
    if (!ready_ || !online()) return;  // tick() opens it once possible
    loadHistory();
    if (historyLoaded_) {
      openMsgStream();
      sendPresence();
      markRead(slug);
    }
  }

  void loadHistory() {
    std::vector<CircMessage> msgs;
    int64_t cursor = -1;
    ApiError e = api_.history(roomKey(room_), 0, msgs, cursor);
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
    ApiError e = api_.history(roomKey(room_), oldest, msgs, cursor);
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
    RtdbQuery q{true, newest, newest < 0 ? 50 : 1000};
    std::string url = rtdbStreamUrl(t.rtdbUrl, "chat_messages/" + roomKey(room_), t.idToken, q);
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
    if (!msgSse_.running()) openMsgStream();
    if (presenceWanted_ && !presSse_.running()) openPresStream();
    if (!presenceSent_) sendPresence();
  }

  // ---- presence and read state ----

  void sendPresence() {
    if (room_.empty() || !screenOn_) return;
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

  void watch() {
    refreshRooms();
    std::vector<Mention> ms;
    ApiError e = api_.mentions(ms);
    if (!e.ok()) {
      checkAuthError(e);
      return;
    }
    for (auto& m : ms) {
      if (!seenMentions_.insert(m.id).second) continue;
      pendingMentions_.emplace(m.roomSlug, m.id);
      post(EvtType::MentionAlert, 0,
           "@" + m.actorUsername + " in #" + m.roomSlug + ": " + m.messageContent);
    }
    if (seenMentions_.size() > 200) seenMentions_.clear();
  }

  // ---- commands ----

  bool needNet(const Command& c) {
    if (online() && ready_) return true;
    banner(online() ? "Not signed in" : "Offline", false);
    if (c.type == CmdType::Send) post(EvtType::SendDone, 0, online() ? "Not signed in" : "Offline", false);
    return false;
  }

  void handle(Command& c) {
    switch (c.type) {
      case CmdType::Activity:
        lastActivity_ = c.n;
        return;
      case CmdType::OpenRoom:
        openRoom(c.a);
        return;
      case CmdType::CloseRoom:
        closeRoom();
        return;
      case CmdType::ScreenOff:
        screenOn_ = false;
        stopStreams();
        if (!room_.empty() && presenceSent_ && online()) api_.leave(roomKey(room_));
        presenceSent_ = false;
        setConn(0);
        return;
      case CmdType::ScreenOn:
        screenOn_ = true;
        backoffMs_ = 1000;
        reconnectAt_ = 0;
        downSince_ = 0;
        return;  // tick() reopens the streams
      case CmdType::OpenPresence:
        presenceWanted_ = true;
        if (needNet(c) && !room_.empty()) {
          seedPresence();
          if (!presSse_.running()) openPresStream();
        }
        return;
      case CmdType::ClosePresence:
        presenceWanted_ = false;
        presSse_.stop();
        return;
      case CmdType::Login:
        login(c);
        return;
      case CmdType::Logout:
        closeRoom();
        api_.signOut();
        self_.clear();
        ready_ = false;
        post(EvtType::AuthState, auth_ = 0);
        return;
      default:
        break;
    }
    if (!needNet(c)) return;
    switch (c.type) {
      case CmdType::Send: {
        std::string id, reply;
        ApiError e = api_.send(roomKey(c.a), c.b, id, reply);
        if (e.ok()) {
          post(EvtType::SendDone);
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
        ApiError e = api_.deleteMessage(roomKey(room_), c.a);
        if (!e.ok()) banner(e.message, false);
        break;
      }
      case CmdType::Flag: {
        ApiError e = api_.flag(roomKey(room_), c.a, c.b);
        banner(e.ok() ? "Reported" : e.message, e.ok());
        break;
      }
      case CmdType::MarkRead:
        markRead(c.a.empty() ? room_ : c.a);
        break;
      case CmdType::RefreshRooms:
        refreshRooms();
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
    if (!online()) {
      post(EvtType::AuthState, auth_, "Offline", false);
    } else {
      Tokens t;
      ApiError e = api_.login(c.a, c.b, t);
      if (!e.ok()) {
        post(EvtType::AuthState, auth_, e.message, false);
      } else {
        email_ = c.a;
        settings::saveAccount(c.a, t.refreshToken);
        ready_ = false;
        nextBootstrap_ = 0;
        bootstrap();
      }
    }
    std::fill(c.b.begin(), c.b.end(), '\0');
  }

  // ---- timers ----

  void tick() {
    wifimgr::reconnectWithBackoff();
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

    if (msgLive_.exchange(false)) {
      backoffMs_ = 1000;
      downSince_ = 0;
      pausedBanner_ = false;
      setConn(2);
    }

    if (!room_.empty() && screenOn_) {
      if (msgDown_.exchange(false)) scheduleReconnect();
      bool needs = !historyLoaded_ || !msgSse_.running() ||
                   (presenceWanted_ && !presSse_.running());
      if (needs && now >= reconnectAt_) {
        if (msgSse_.running() == false && reconnectAt_ == 0) setConn(1);
        reconnectAt_ = now + backoffMs_;  // until the stream reports live
        reconnect();
      }
      if (downSince_ && !pausedBanner_ && now - downSince_ > kPausedBannerMs) {
        pausedBanner_ = true;
        banner("Chat paused, reconnecting", false);
      }
      if (now >= nextHeartbeat_) sendPresence();
    }

    if (now >= nextWatch_) {
      nextWatch_ = now + kWatchMs;
      watch();
    }
  }

  HttpsClient http_;
  CsApi api_;
  SseClient msgSse_, presSse_;
  std::string email_, self_, room_;
  std::map<std::string, std::string> roomIds_;  // slug → id
  std::set<std::string> seenMentions_;
  std::multimap<std::string, std::string> pendingMentions_;  // slug → notification id

  std::atomic<bool> msgLive_{false}, msgDown_{false}, authRevoked_{false};
  bool ready_ = false, historyLoaded_ = false, presenceSent_ = false, presenceWanted_ = false;
  bool screenOn_ = true, wasOnline_ = false, pausedBanner_ = false;
  int conn_ = -1, auth_ = -1;
  int64_t lastActivity_ = 0, heartbeatMs_ = 30000, nextHeartbeat_ = 0, nextWatch_ = 0;
  int64_t nextBootstrap_ = 0, reconnectAt_ = 0, downSince_ = 0, backoffMs_ = 1000;
};

void taskMain(void* arg) {
  static_cast<Worker*>(arg)->loop();
}

}  // namespace

void start(const Settings& s) {
  wifimgr::begin(s);
  static Worker* w = new Worker(s);
  xTaskCreatePinnedToCore(taskMain, "net", 12288, w, 3, nullptr, 0);
}

}  // namespace rc::networker

#endif
