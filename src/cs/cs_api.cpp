#include "cs/cs_api.h"

#include <ArduinoJson.h>
#include <esp_timer.h>

#include <ctime>

#include "cs/token.h"
#include "feed/feed_parse.h"
#include "guild/guild_parse.h"
#include "store/settings.h"

namespace rc {

namespace {

int64_t monoMs() { return esp_timer_get_time() / 1000; }

std::string enc(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string o;
  for (unsigned char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      o += char(c);
    } else {
      o += '%';
      o += hex[c >> 4];
      o += hex[c & 15];
    }
  }
  return o;
}

ApiError transport() { return ApiError{0, "NETWORK", "Network error"}; }

ApiError tooLarge() { return ApiError{0, "TOO_LARGE", "Response too large"}; }

ApiError transport(const HttpResult& r) { return r.overflow ? tooLarge() : transport(); }

ApiError badBody(int http) { return ApiError{http, "BAD_RESPONSE", "Unexpected response"}; }

std::string json(const JsonDocument& d) {
  std::string s;
  serializeJson(d, s);
  return s;
}

}  // namespace

CsApi::CsApi(HttpsClient& h)
    : http_(h),
      sendL_(makeCircSendLimiter()),
      presenceL_(makePresenceLimiter()),
      deleteL_(makeDeleteLimiter()),
      flagL_(makeFlagLimiter()),
      readL_(makeMarkReadLimiter()),
      resendL_({{1, 60000}, {5, 3600000}}),
      cmailSendL_(makeCmailSendLimiter()),
      startL_(makeStartConversationLimiter()),
      cmailReadL_(makeMarkReadLimiter()),
      typingL_(makeTypingLimiter()),
      readAllL_(makeMarkReadLimiter()),
      entryL_(makeEntryLimiter()),
      replyL_(makeReplyLimiter()),
      bookmarkL_(makeBookmarkLimiter()),
      watchL_(makeWatchLimiter()),
      membershipL_(makeMembershipLimiter()) {}

void CsApi::adopt(const Tokens& t) {
  tokens_ = t;
  refreshDue_ = refreshDueMonoMs(monoMs(), int64_t(time(nullptr)), jwtExpSeconds(t.idToken));
}

void CsApi::setRefreshToken(const std::string& t) {
  tokens_ = Tokens{};
  tokens_.refreshToken = t;
  refreshDue_ = 0;
}

void CsApi::signOut() {
  tokens_ = Tokens{};
  refreshDue_ = 0;
  settings::clearToken();
}

ApiError CsApi::limited(RateLimiter& l) {
  int64_t now = monoMs();
  if (l.tryAcquire(now)) return ApiError{200, "", ""};
  int64_t s = (l.retryAfterMs(now) + 999) / 1000;
  if (s < 1) s = 1;
  return ApiError{429, "LOCAL_RATE_LIMIT", "Slow down: try in " + std::to_string(s) + "s"};
}

ApiError CsApi::call(const char* method, const std::string& path, const std::string& body,
                     HttpResult& out) {
  if (!signedIn()) return ApiError{401, "SIGNED_OUT", "Not signed in"};
  if (tokens_.idToken.empty()) {
    Tokens t = tokens_;
    ApiError e = refresh(t);
    if (!e.ok()) return e;
  }
  out = http_.request(method, path, body, tokens_.idToken);
  if (out.transportError) return transport(out);
  if (out.status == 401) {
    // Auth expiry: refresh once and repeat the same call once.
    Tokens t = tokens_;
    ApiError e = refresh(t);
    if (!e.ok()) return e;
    out = http_.request(method, path, body, tokens_.idToken);
    if (out.transportError) return transport(out);
  }
  return parseError(out.status, out.body);
}

ApiError CsApi::login(const std::string& email, const std::string& password, Tokens& out) {
  JsonDocument d;
  d["email"] = email;
  d["password"] = password;
  std::string body = json(d);
  HttpResult r = http_.request("POST", "/v1/auth/login", body, "");
  // Don't leave the password in freed memory.
  for (char& c : body) c = 0;
  if (r.transportError) return transport();
  ApiError e = parseError(r.status, r.body);
  if (!e.ok()) return e;
  Tokens t;
  if (!parseTokens(r.body, t) || t.idToken.empty() || t.refreshToken.empty())
    return badBody(r.status);
  adopt(t);
  out = t;
  return e;
}

ApiError CsApi::refresh(Tokens& io) {
  if (io.refreshToken.empty()) return ApiError{401, "SIGNED_OUT", "Not signed in"};
  JsonDocument d;
  d["refreshToken"] = io.refreshToken;
  HttpResult r = http_.request("POST", "/v1/auth/refresh", json(d), "");
  if (r.transportError) return refreshErr_ = transport();
  ApiError e = parseError(r.status, r.body);
  if (!e.ok()) return refreshErr_ = e;
  Tokens t = io;
  if (!parseTokens(r.body, t) || t.idToken.empty()) return refreshErr_ = badBody(r.status);
  io = t;
  adopt(t);
  return refreshErr_ = e;
}

ApiError CsApi::resendVerification(const std::string& idToken) {
  ApiError l = limited(resendL_);
  if (!l.ok()) return l;
  JsonDocument d;
  d["idToken"] = idToken;
  HttpResult r = http_.request("POST", "/v1/auth/resend-verification", json(d), "");
  if (r.transportError) return transport();
  return parseError(r.status, r.body);
}

bool CsApi::ensureFresh(int64_t nowMono, int64_t epochSec) {
  (void)epochSec;  // adopt() reads the clock itself when the token arrives
  if (!signedIn()) return false;
  if (!tokens_.idToken.empty() && nowMono < refreshDue_) return true;
  Tokens t = tokens_;
  return refresh(t).ok();
}

ApiError CsApi::me(std::string& username) {
  HttpResult r;
  ApiError e = call("GET", "/v1/users/me", "", r);
  if (!e.ok()) return e;
  JsonDocument d;
  if (deserializeJson(d, r.body)) return badBody(r.status);
  JsonVariantConst u = d["data"]["username"];
  if (!u.is<const char*>()) u = d["username"];
  if (!u.is<const char*>()) return badBody(r.status);
  username = u.as<const char*>();
  return e;
}

ApiError CsApi::rooms(std::vector<RoomInfo>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/circ", "", r);
  if (e.ok() && !parseRooms(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::history(const std::string& slug, int64_t before, std::vector<CircMessage>& out,
                        int64_t& cursor) {
  std::string path = "/v1/circ/" + enc(slug) + "?limit=50";
  if (before > 0) path += "&before=" + std::to_string(before);
  HttpResult r;
  ApiError e = call("GET", path, "", r);
  if (e.ok() && !parseHistory(r.body, out, cursor)) return badBody(r.status);
  return e;
}

ApiError CsApi::send(const std::string& slug, const std::string& utf8, std::string& messageId,
                     std::string& reply) {
  ApiError l = limited(sendL_);
  if (!l.ok()) return l;
  JsonDocument d;
  d["content"] = utf8;
  HttpResult r;
  ApiError e = call("POST", "/v1/circ/" + enc(slug), json(d), r);
  // The message may already be posted, so a bad body is not reported as a
  // failure the user would resend.
  if (e.ok()) parseSendResult(r.body, messageId, reply);
  return e;
}

ApiError CsApi::deleteMessage(const std::string& slug, const std::string& id) {
  ApiError l = limited(deleteL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/circ/" + enc(slug) + "/messages/" + enc(id), "", r);
}

ApiError CsApi::flag(const std::string& slug, const std::string& id, const std::string& reason) {
  ApiError l = limited(flagL_);
  if (!l.ok()) return l;
  std::string body;
  if (!reason.empty()) {
    JsonDocument d;
    d["reason"] = reason;
    body = json(d);
  }
  HttpResult r;
  return call("POST", "/v1/circ/" + enc(slug) + "/messages/" + enc(id) + "/flag", body, r);
}

ApiError CsApi::markRead(const std::string& slug) {
  ApiError l = limited(readL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/circ/" + enc(slug) + "/read", "", r);
}

ApiError CsApi::users(const std::string& slug, std::vector<PresenceEntry>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/circ/" + enc(slug) + "/users", "", r);
  if (e.ok() && !parseUsers(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::presence(const std::string& slug, int64_t lastActivity, PresenceCadence& out) {
  ApiError l = limited(presenceL_);
  if (!l.ok()) return l;
  JsonDocument d;
  d["lastActivity"] = lastActivity;
  HttpResult r;
  ApiError e = call("POST", "/v1/circ/" + enc(slug) + "/presence", json(d), r);
  if (e.ok()) parsePresenceAck(r.body, out);  // keeps the defaults on a bad body
  return e;
}

ApiError CsApi::leave(const std::string& slug) {
  ApiError l = limited(presenceL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/circ/" + enc(slug) + "/presence", "", r);
}

ApiError CsApi::conversations(std::vector<ConversationInfo>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/cmail", "", r);
  if (e.ok() && !parseConversations(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::startConversation(const std::string& username, ConversationInfo& out) {
  ApiError l = limited(startL_);
  if (!l.ok()) return l;
  JsonDocument d;
  d["recipientUsername"] = username;
  HttpResult r;
  ApiError e = call("POST", "/v1/cmail", json(d), r);
  if (e.ok() && !parseStartConversation(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::cmailHistory(const std::string& id, int64_t before, std::vector<CircMessage>& out,
                             int64_t& cursor) {
  std::string path = "/v1/cmail/" + enc(id) + "?limit=50";
  if (before > 0) path += "&before=" + std::to_string(before);
  HttpResult r;
  ApiError e = call("GET", path, "", r);
  if (e.ok() && !parseHistory(r.body, out, cursor)) return badBody(r.status);
  return e;
}

ApiError CsApi::cmailSend(const std::string& id, const std::string& utf8, std::string& messageId) {
  ApiError l = limited(cmailSendL_);
  if (!l.ok()) return l;
  JsonDocument d;
  d["content"] = utf8;
  HttpResult r;
  ApiError e = call("POST", "/v1/cmail/" + enc(id), json(d), r);
  if (e.ok()) {
    std::string reply;
    parseSendResult(r.body, messageId, reply);
  }
  return e;
}

ApiError CsApi::cmailRead(const std::string& id) {
  ApiError l = limited(cmailReadL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/cmail/" + enc(id) + "/read", "", r);
}

ApiError CsApi::cmailTyping(const std::string& id, TypingCadence& out) {
  ApiError l = limited(typingL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/cmail/" + enc(id) + "/typing", "", r);
  if (e.ok()) parseTypingAck(r.body, out);
  return e;
}

ApiError CsApi::cmailTypingClear(const std::string& id) {
  ApiError l = limited(typingL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/cmail/" + enc(id) + "/typing", "", r);
}

ApiError CsApi::mutedUsers(MutedUsers& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/settings", "", r);
  if (e.ok() && !parseMutedUsers(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::mentions(std::vector<Mention>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/notifications?read=false&type=chat_mention,reply,thread_reply,post_mention,"
                     "reply_mention,post_cooldown,guild_new_thread&limit=20", "", r);
  if (e.ok() && !parseMentions(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::markNotificationRead(const std::string& id) {
  HttpResult r;
  return call("PATCH", "/v1/notifications/" + enc(id), "", r);
}

ApiError CsApi::markAllNotificationsRead() {
  ApiError l = limited(readAllL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/notifications/read-all", "", r);
}

namespace {

std::string pageQuery(int limit, const std::string& cursor) {
  std::string q = "?limit=" + std::to_string(limit);
  if (!cursor.empty()) q += "&cursor=" + enc(cursor);
  return q;
}

std::string reasonBody(const std::string& reason) {
  if (reason.empty()) return "";
  JsonDocument d;
  d["reason"] = reason;
  return json(d);
}

}  // namespace

ApiError CsApi::feed(const std::string& cursor, int limit, std::vector<Entry>& out,
                     std::string& next) {
  HttpResult r;
  ApiError e = call("GET", "/v1/posts" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseFeedPage(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::entry(const std::string& id, Entry& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/posts/" + enc(id), "", r);
  if (e.ok() && !parseEntry(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::replies(const std::string& postId, const std::string& cursor, int limit,
                        std::vector<Reply>& out, std::string& next) {
  HttpResult r;
  ApiError e =
      call("GET", "/v1/posts/" + enc(postId) + "/replies" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseReplies(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::reply(const std::string& id, Reply& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/replies/" + enc(id), "", r);
  if (e.ok() && !parseReply(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::postEntry(const std::string& body, std::string& postId) {
  ApiError l = limited(entryL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/posts", body, r);
  // Already posted: a bad body is not a failure the user would resend.
  if (e.ok()) parseCreatedId(r.body, postId);
  return e;
}

ApiError CsApi::postReply(const std::string& body, std::string& replyId) {
  ApiError l = limited(replyL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/replies", body, r);
  if (e.ok()) parseCreatedId(r.body, replyId);
  return e;
}

ApiError CsApi::deleteEntry(const std::string& id) {
  ApiError l = limited(deleteL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/posts/" + enc(id), "", r);
}

ApiError CsApi::deleteReply(const std::string& id) {
  ApiError l = limited(deleteL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/replies/" + enc(id), "", r);
}

ApiError CsApi::flagEntry(const std::string& id, const std::string& reason) {
  ApiError l = limited(flagL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/posts/" + enc(id) + "/flag", reasonBody(reason), r);
}

ApiError CsApi::flagReply(const std::string& id, const std::string& reason) {
  ApiError l = limited(flagL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/replies/" + enc(id) + "/flag", reasonBody(reason), r);
}

ApiError CsApi::watchStatus(const std::string& postId, bool& watching) {
  HttpResult r;
  ApiError e = call("GET", "/v1/posts/" + enc(postId) + "/watch", "", r);
  if (e.ok() && !parseWatching(r.body, watching)) return badBody(r.status);
  return e;
}

ApiError CsApi::setWatch(const std::string& postId, bool on) {
  ApiError l = limited(watchL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call(on ? "POST" : "DELETE", "/v1/posts/" + enc(postId) + "/watch", "", r);
}

ApiError CsApi::watches(const std::string& cursor, std::vector<std::string>& postIds,
                        std::string& next, int limit) {
  HttpResult r;
  ApiError e = call("GET", "/v1/watches" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseWatches(r.body, postIds, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::bookmarks(const std::string& cursor, std::vector<Bookmark>& out,
                          std::string& next, int limit) {
  HttpResult r;
  ApiError e = call("GET", "/v1/bookmarks" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseBookmarks(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::addBookmark(const std::string& postId, const std::string& replyId,
                            std::string& bookmarkId) {
  ApiError l = limited(bookmarkL_);
  if (!l.ok()) return l;
  JsonDocument d;
  if (replyId.empty()) {
    d["postId"] = postId;
    d["type"] = "post";
  } else {
    d["replyId"] = replyId;
    d["type"] = "reply";
  }
  HttpResult r;
  ApiError e = call("POST", "/v1/bookmarks", json(d), r);
  if (e.ok() && !parseCreatedId(r.body, bookmarkId)) bookmarkId.clear();  // saved, id unknown
  return e;
}

ApiError CsApi::removeBookmark(const std::string& bookmarkId) {
  ApiError l = limited(bookmarkL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("DELETE", "/v1/bookmarks/" + enc(bookmarkId), "", r);
}


ApiError CsApi::guilds(const std::string& cursor, int limit, std::vector<Guild>& out,
                       std::string& next) {
  HttpResult r;
  ApiError e = call("GET", "/v1/guilds" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseGuildPage(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::guild(const std::string& slug, Guild& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/guilds/" + enc(slug), "", r);
  if (e.ok() && !parseGuild(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::guildMembers(const std::string& slug, const std::string& cursor, int limit,
                             std::vector<Member>& out, std::string& next) {
  HttpResult r;
  ApiError e =
      call("GET", "/v1/guilds/" + enc(slug) + "/members" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseMembers(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::guildThreads(const std::string& slug, const std::string& cursor, int limit,
                             std::vector<Entry>& out, std::string& next) {
  HttpResult r;
  ApiError e = call("GET", "/v1/guilds/" + enc(slug) + "/posts" + pageQuery(limit, cursor), "", r);
  if (e.ok() && !parseFeedPage(r.body, out, next)) return badBody(r.status);
  return e;
}

ApiError CsApi::myGuilds(std::vector<MyGuild>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/users/me/guilds", "", r);
  if (e.ok() && !parseMyGuilds(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::postGuildThread(const std::string& slug, const std::string& body,
                                std::string& postId) {
  ApiError l = limited(entryL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/guilds/" + enc(slug) + "/posts", body, r);
  if (e.ok()) parseCreatedId(r.body, postId);
  return e;
}

ApiError CsApi::joinGuild(const std::string& slug, std::string& role) {
  ApiError l = limited(membershipL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/guilds/" + enc(slug) + "/join", "", r);
  // Already joined: an unreadable role is not a failure the user would resend.
  if (e.ok()) parseGuildRole(r.body, role);
  return e;
}

ApiError CsApi::promoteGuild(const std::string& slug, std::string& role) {
  ApiError l = limited(membershipL_);
  if (!l.ok()) return l;
  HttpResult r;
  ApiError e = call("POST", "/v1/guilds/" + enc(slug) + "/promote", "", r);
  if (e.ok()) parseGuildRole(r.body, role);
  return e;
}

ApiError CsApi::leaveGuild(const std::string& slug) {
  ApiError l = limited(membershipL_);
  if (!l.ok()) return l;
  HttpResult r;
  return call("POST", "/v1/guilds/" + enc(slug) + "/leave", "", r);
}

}  // namespace rc
