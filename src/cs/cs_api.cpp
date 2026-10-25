#include "cs/cs_api.h"

#include <ArduinoJson.h>
#include <esp_timer.h>

#include <ctime>

#include "cs/token.h"
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
      resendL_({{1, 60000}, {5, 3600000}}) {}

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
  if (out.transportError) return transport();
  if (out.status == 401) {
    // Auth expiry: refresh once and repeat the same call once.
    Tokens t = tokens_;
    ApiError e = refresh(t);
    if (!e.ok()) return e;
    out = http_.request(method, path, body, tokens_.idToken);
    if (out.transportError) return transport();
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

ApiError CsApi::mutedUsers(MutedUsers& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/settings", "", r);
  if (e.ok() && !parseMutedUsers(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::mentions(std::vector<Mention>& out) {
  HttpResult r;
  ApiError e = call("GET", "/v1/notifications?read=false&type=chat_mention&limit=20", "", r);
  if (e.ok() && !parseMentions(r.body, out)) return badBody(r.status);
  return e;
}

ApiError CsApi::markNotificationRead(const std::string& id) {
  HttpResult r;
  return call("PATCH", "/v1/notifications/" + enc(id), "", r);
}

}  // namespace rc
