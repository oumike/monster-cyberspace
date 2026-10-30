#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "chat/message.h"
#include "chat/presence.h"
#include "chat/typing.h"

namespace rc {

struct Tokens {
  std::string idToken, refreshToken, rtdbUrl;
};

struct ApiError {
  int http = 0;
  std::string code, message;
  bool ok() const { return http >= 200 && http < 300; }
};

// Reads the {"error":{code,message}} envelope. A non-JSON body (a proxy's
// HTML page) gives code "HTTP_<n>" and message "Server error".
ApiError parseError(int http, std::string_view body);

// Login and refresh. Refresh omits refreshToken; the existing one is kept.
// `out` is untouched on failure.
bool parseTokens(std::string_view body, Tokens& out);

struct RoomInfo {
  std::string id, slug, name;
  int64_t lastMessageAt = 0;
  int sortOrder = 0, onlineCount = 0;
};
bool parseRooms(std::string_view body, std::vector<RoomInfo>& out);

// cursor is the oldest message's timestamp (number or numeric string), or -1
// when null or absent.
bool parseHistory(std::string_view body, std::vector<CircMessage>& out, int64_t& cursor);

// Users listed by GET /v1/circ/:room/users are online by definition.
bool parseUsers(std::string_view body, std::vector<PresenceEntry>& out);

struct PresenceCadence {
  int64_t heartbeatMs = 30000, staleAfterMs = 180000, idleAfterMs = 600000;
};
bool parsePresenceAck(std::string_view body, PresenceCadence& out);

// `reply` is set by commands that post nothing (/help, /mute...).
bool parseSendResult(std::string_view body, std::string& messageId, std::string& reply);

struct Mention {
  std::string id, roomSlug, roomName, actorUsername, messageContent;
  std::string guildName;  // guild_new_thread: metadata.guildName
  // Feed notifications: preview is metadata.replyContent, else postContent,
  // else the top-level reason.
  std::string type, targetId, replyId, preview;
};
bool parseMentions(std::string_view body, std::vector<Mention>& out);

bool parseUnreadCount(std::string_view body, int& count, bool& exact);

// GET /v1/settings → mutedUsersByRoom: room key (id or slug) → lower-cased
// usernames. Each room may hold an array of names or a {name: bool} object.
using MutedUsers = std::map<std::string, std::set<std::string>>;
bool parseMutedUsers(std::string_view body, MutedUsers& out);

// C-Mail. A conversation with its partner and unread state.
struct ConversationInfo {
  std::string id, partnerId, partner, lastMessage;
  int64_t lastMessageAt = 0;
  int unread = 0;
  bool partnerDeleted = false;
};
// GET /v1/cmail, in server order (unread first, then newest).
bool parseConversations(std::string_view body, std::vector<ConversationInfo>& out);
// POST /v1/cmail: sets id, partnerId and partner.
bool parseStartConversation(std::string_view body, ConversationInfo& out);
// POST /v1/cmail/:id/typing. `out` keeps its defaults on failure.
bool parseTypingAck(std::string_view body, TypingCadence& out);

}  // namespace rc
