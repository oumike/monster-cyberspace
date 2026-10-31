#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "chat/chat_store.h"
#include "chat/presence.h"
#include "cs/api_parse.h"

// Messages kept; boards without PSRAM (the X4) set a smaller cap.
#ifndef RC_CHAT_CAP
#define RC_CHAT_CAP 1000
#endif

namespace rc {

// Everything the UI reads and the network side writes, behind one mutex.
struct ChatState {
  ChatStore store{RC_CHAT_CAP};
  PresenceMap presence;
  std::vector<RoomInfo> rooms;
  PresenceCadence cadence;
  std::string roomSlug;                  // the open room, "" when none
  std::string self;                      // signed-in username
  std::map<std::string, int64_t> readAt; // slug → lastMessageAt when last read
  std::set<std::string> revealed;        // spoiler ids the user revealed
  bool noOlder = false;                  // history exhausted for this room
  MutedUsers muted;                      // room id or slug → lower-cased names

  // C-Mail. When dm is true, roomSlug holds the open conversation's id.
  bool dm = false;
  std::vector<ConversationInfo> conversations;
  TypingMap typing;                      // the open conversation's dm_presence
  TypingCadence typingCadence;
};

class LockedChat {
 public:
  // Runs f(ChatState&) under the lock. Keep f short: no I/O.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  ChatState state_;
};

LockedChat& lockedChat();

// Unread C-Mail across every conversation. Takes the lock.
int cmailUnread();

}  // namespace rc
