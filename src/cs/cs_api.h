#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cs/api_parse.h"
#include "cs/rate_limiter.h"
#include "feed/entry.h"
#include "guild/guild.h"
#include "net/https_client.h"
#include "people/profile.h"

namespace rc {

// The Cyberspace REST API. Owned by the network task; not thread-safe.
// Writes are checked against local rate limits first and are never retried,
// except once after a 401, when the token is refreshed.
class CsApi {
 public:
  explicit CsApi(HttpsClient&);

  // Auth. The password is used for this request only and never kept.
  ApiError login(const std::string& email, const std::string& password, Tokens& out);
  ApiError refresh(Tokens& io);
  ApiError resendVerification(const std::string& idToken);
  // Refreshes when signed in and monoMs >= the refresh due time (or there is
  // no idToken yet). False when signed out or the refresh failed.
  bool ensureFresh(int64_t monoMs, int64_t epochSec);
  // Forces the next ensureFresh() to refresh (for RTDB auth_revoked).
  void expireNow() { refreshDue_ = 0; }
  // The result of the latest refresh (ok when none failed).
  const ApiError& lastRefreshError() const { return refreshErr_; }

  // The signed-in username (GET /v1/users/me), and the usernames I blocked
  // on the website when `blocked` is set.
  ApiError me(std::string& username, std::vector<std::string>* blocked = nullptr);

  // cIRC.
  ApiError rooms(std::vector<RoomInfo>&);
  ApiError history(const std::string& slug, int64_t before, std::vector<CircMessage>&,
                   int64_t& cursor);
  ApiError send(const std::string& slug, const std::string& utf8, std::string& messageId,
                std::string& reply);
  ApiError deleteMessage(const std::string& slug, const std::string& id);
  ApiError flag(const std::string& slug, const std::string& id, const std::string& reason);
  ApiError markRead(const std::string& slug);
  ApiError users(const std::string& slug, std::vector<PresenceEntry>&);
  ApiError presence(const std::string& slug, int64_t lastActivity, PresenceCadence&);
  ApiError leave(const std::string& slug);

  // C-Mail.
  ApiError conversations(std::vector<ConversationInfo>&);
  ApiError startConversation(const std::string& username, ConversationInfo& out);
  ApiError cmailHistory(const std::string& id, int64_t before, std::vector<CircMessage>&,
                        int64_t& cursor);
  ApiError cmailSend(const std::string& id, const std::string& utf8, std::string& messageId);
  ApiError cmailRead(const std::string& id);
  ApiError cmailTyping(const std::string& id, TypingCadence& out);
  ApiError cmailTypingClear(const std::string& id);

  // Notifications.
  // GET /v1/settings, reading only mutedUsersByRoom.
  ApiError mutedUsers(MutedUsers&);
  ApiError mentions(std::vector<Mention>&);
  ApiError markNotificationRead(const std::string& id);
  // POST /v1/notifications/read-all, once (up to 5,000 per call).
  ApiError markAllNotificationsRead();

  // Feed. List cursors are "" at the end. `limit` is the page size.
  ApiError feed(const std::string& cursor, int limit, std::vector<Entry>&, std::string& next);
  ApiError entry(const std::string& id, Entry&);
  ApiError replies(const std::string& postId, const std::string& cursor, int limit,
                   std::vector<Reply>&, std::string& next);
  ApiError reply(const std::string& id, Reply&);
  // `json` is a body built by feed/compose.
  ApiError postEntry(const std::string& json, std::string& postId);
  ApiError postReply(const std::string& json, std::string& replyId);
  ApiError deleteEntry(const std::string& id);
  ApiError deleteReply(const std::string& id);
  ApiError flagEntry(const std::string& id, const std::string& reason);
  ApiError flagReply(const std::string& id, const std::string& reason);
  ApiError watchStatus(const std::string& postId, bool& watching);
  ApiError setWatch(const std::string& postId, bool on);  // POST or DELETE
  ApiError watches(const std::string& cursor, std::vector<std::string>& postIds, std::string& next,
                   int limit = 10);
  ApiError bookmarks(const std::string& cursor, std::vector<Bookmark>&, std::string& next,
                     int limit = 50);
  // A reply bookmark when replyId is set, else an entry bookmark.
  ApiError addBookmark(const std::string& postId, const std::string& replyId,
                       std::string& bookmarkId);
  ApiError removeBookmark(const std::string& bookmarkId);

  // Guilds. Cursors are "" at the end; a 404 from guild() reaches the caller as http 404.
  ApiError guilds(const std::string& cursor, int limit, std::vector<Guild>&, std::string& next);
  ApiError guild(const std::string& slug, Guild&);
  ApiError guildMembers(const std::string& slug, const std::string& cursor, int limit,
                        std::vector<Member>&, std::string& next);
  ApiError guildThreads(const std::string& slug, const std::string& cursor, int limit,
                        std::vector<Entry>&, std::string& next);
  ApiError myGuilds(std::vector<MyGuild>&);
  ApiError postGuildThread(const std::string& slug, const std::string& json, std::string& postId);
  ApiError joinGuild(const std::string& slug, std::string& role);
  ApiError promoteGuild(const std::string& slug, std::string& role);
  ApiError leaveGuild(const std::string& slug);

  // People. There is no block endpoint: blocking happens on the website.
  // A 404 from profile() reaches the caller as http 404.
  ApiError profile(const std::string& username, Profile&);
  // My following list (GET /v1/follows?type=following).
  ApiError following(const std::string& cursor, int limit, std::vector<Follow>&,
                     std::string& next);
  ApiError follow(const std::string& userId, std::string& followId);
  ApiError unfollow(const std::string& followId);
  // Only an accepted (or server rate-limited) poke counts locally, as on the
  // server: a refused one (400/403/404) doesn't use up the hour.
  ApiError poke(const std::string& username);
  ApiError userPosts(const std::string& username, const std::string& cursor, int limit,
                     std::vector<Entry>&, std::string& next);

  const Tokens& tokens() const { return tokens_; }
  bool signedIn() const { return !tokens_.refreshToken.empty(); }
  // Restores a session from the refresh token saved in NVS.
  void setRefreshToken(const std::string& t);
  // Clears the tokens here and in NVS.
  void signOut();

 private:
  ApiError call(const char* method, const std::string& path, const std::string& body,
                HttpResult& out);
  ApiError limited(RateLimiter& l);
  void adopt(const Tokens& t);

  HttpsClient& http_;
  Tokens tokens_;
  int64_t refreshDue_ = 0;
  ApiError refreshErr_{200, "", ""};
  RateLimiter sendL_, presenceL_, deleteL_, flagL_, readL_, resendL_;
  RateLimiter cmailSendL_, startL_, cmailReadL_, typingL_, readAllL_;
  RateLimiter entryL_, replyL_, bookmarkL_, watchL_, membershipL_;
  RateLimiter followL_, unfollowL_, pokeL_;
};

}  // namespace rc
