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
  SetupAp,         // n = 1 on, 0 off
  WifiScan,        // answered by WifiScanned
  WifiJoin,        // a = SSID; n = 1: b = password to save (zeroed after use), 0: the saved one
  WifiForget,      // a = SSID
  ResendVerification,
  ScreenOff,
  ScreenOn,
  Activity,        // n = epoch ms of the latest key press
  // C-Mail. A conversation is open like a room; CloseRoom closes it.
  OpenConversation,      // a = conversation id
  StartConversation,     // a = username
  RefreshConversations,
  Typing,                // a = conversation id, n = 1 typing, 0 stopped
  // Feed.
  OpenFeed,
  CloseFeed,
  FeedMore,
  FeedRefresh,
  OpenEntry,             // a = postId, b = notification id or ""
  MoreReplies,
  PostEntry,             // a = JSON
  PostReply,             // a = JSON
  DeleteEntry,           // a = id
  DeleteReply,           // a = id
  FlagEntry,             // a = id, b = reason
  FlagReply,             // a = id, b = reason
  Bookmark,              // a = postId, b = replyId ("" for the entry)
  Unbookmark,            // a = target id (post or reply)
  Watch,                 // a = postId
  Unwatch,               // a = postId
  OpenWatched,           // n = 1 to append the next page
  OpenBookmarks,         // n = 1 to append the next page
  // Guilds.
  OpenGuilds,            // n = 1 to append the next page
  OpenGuild,             // a = slug; loads the guild and its first threads
  GuildThreadsMore,      // a = slug
  CloseGuild,
  OpenMembers,           // a = slug; n = 1 to append the next page
  PostGuildThread,       // a = slug, b = JSON
  JoinGuild,             // a = slug
  LeaveGuild,            // a = slug
  PromoteGuild,          // a = slug
  // People.
  OpenProfile,           // a = username
  Follow,                // a = userId, b = username
  Unfollow,              // a = userId, b = username
  Poke,                  // a = username
  OpenUserPosts,         // a = username; n = 1 to append the next page
  MarkAllRead,           // Unread: every notification and unread C-Mail
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
  PrefsChanged,     // from the web page: code = TextSize index; ok = buzzer; text = theme key
  RotationChanged,  // from the web page: code = rotation 0-3 (ui/rotation.h)
  ConversationsUpdated,
  ConversationStarted,  // ok: text = conversation id; else text = error
  TypingChanged,
  CmailAlert,           // text = "C-Mail from @name: preview"
  // Feed.
  FeedUpdated,
  EntryUpdated,         // ok=false when the entry is gone or failed to load
  RepliesLoaded,
  SideListUpdated,
  FeedWriteDone,        // a reply's result: ok; text = error message when !ok
  EntryPosted,          // text = postId; !ok: text = error (the entry wasn't posted)
  FeedAlert,            // text = banner; code = 1 to beep
  // Guilds.
  GuildsUpdated,        // ok=false on a failed load
  GuildUpdated,         // ok=false; code = 404 when the guild is gone
  MembersUpdated,       // ok=false on a failed load
  GuildWriteDone,       // join/leave/promote: ok; text = error or the success banner
  // People.
  ProfileUpdated,       // ok=false on a failed load; code = 404 when there's no such user
  PeopleWriteDone,      // follow/unfollow/poke: ok; text = error or the success banner
  // The device's Wi-Fi screen.
  WifiScanned,          // text = rc::encodeWifiScan
  // code 0 joining (text = SSID), 1 joined (text = SSID), 2 forgotten
  // (text = SSID); -1 failed, -2 refused: ok=false, text = the message.
  WifiState,
  NotifyPreview,        // from the web page: code = rc::LedColor index, one double blink
  AllRead,              // MarkAllRead: ok when the notifications were marked; text = banner
};

struct Event {
  EvtType type;
  std::string text;
  int code = 0;
  bool ok = true;
  std::string arg;  // MentionAlert: the room slug
  int64_t at = 0;   // MentionAlert: when it was made, epoch ms; 0 unknown
};

}  // namespace rc
