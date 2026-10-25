#pragma once

#include <cstdint>
#include <string>

namespace rc {

// UI → network. Every write command comes from a key press.
enum class CmdType {
  OpenRoom,        // a = slug
  CloseRoom,
  Send,            // a = slug, b = text
  LoadOlder,
  Delete,          // a = message id
  Flag,            // a = message id, b = reason (may be empty)
  MarkRead,        // a = slug
  OpenPresence,
  ClosePresence,
  RefreshRooms,
  Login,           // a = email, b = password (zeroed after use)
  Logout,
  ResendVerification,
  ScreenOff,
  ScreenOn,
  Activity,        // n = epoch ms of the latest key press
};

struct Command {
  CmdType type;
  std::string a, b;
  int64_t n = 0;
};

// Network → UI.
enum class EvtType {
  RoomChanged,      // the store or presence of the open room changed
  RoomsUpdated,
  PresenceUpdated,
  ConnState,        // code: 0 offline, 1 reconnecting, 2 live
  Banner,           // text; ok=false for an error
  MentionAlert,     // text = "@actor in #room: content"
  AuthState,        // code: 0 signed out, 1 signed in, 2 email unverified; text = username or error
  SendDone,         // ok; text = error message when !ok
  OlderLoaded,      // code = messages added
  PrefsChanged,     // from the web page: code = 0 Comfortable, 1 Compact; ok = buzzer
};

struct Event {
  EvtType type;
  std::string text;
  int code = 0;
  bool ok = true;
};

}  // namespace rc
