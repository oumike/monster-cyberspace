#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cs/api_parse.h"
#include "cs/rate_limiter.h"
#include "net/https_client.h"

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

  // The signed-in username (GET /v1/users/me).
  ApiError me(std::string& username);

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

  // Notifications.
  ApiError mentions(std::vector<Mention>&);
  ApiError markNotificationRead(const std::string& id);

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
};

}  // namespace rc
