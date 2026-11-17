// SPDX-License-Identifier: GPL-3.0-or-later
// HttpsClient on libcurl: one easy handle kept between requests, so its
// connection stays alive; the system CA store; the ESP build's timeouts and
// body cap (spec section 4.2).
#include "net/https_client.h"

#include <Arduino.h>
#include <curl/curl.h>

#include <cstring>
#include <mutex>

#include "core_version.h"

namespace rc {

namespace {

constexpr const char* kBase = "https://api.cyberspace.online";
constexpr size_t kBodyCap = 256 * 1024;
constexpr long kTimeoutMs = 15000;

struct Sink {
  std::string body;
  bool overflow = false;
};

size_t onData(char* p, size_t size, size_t n, void* user) {
  auto* s = static_cast<Sink*>(user);
  const size_t len = size * n;
  if (s->body.size() + len > kBodyCap) {
    s->overflow = true;
    return 0;  // CURLE_WRITE_ERROR
  }
  s->body.append(p, len);
  return len;
}

void curlGlobalInit() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

}  // namespace

HttpsClient::~HttpsClient() { close(); }

void HttpsClient::close() {
  if (!handle_) return;
  curl_easy_cleanup(static_cast<CURL*>(handle_));
  handle_ = nullptr;
}

HttpResult HttpsClient::request(const char* method, const std::string& path,
                                const std::string& jsonBody, const std::string& bearer) {
  HttpResult r;
  curlGlobalInit();
  if (!handle_) handle_ = curl_easy_init();
  if (!handle_) {
    r.transportError = true;
    return r;
  }
  CURL* h = static_cast<CURL*>(handle_);
  curl_easy_reset(h);  // keeps the open connection

  const std::string url = std::string(kBase) + path;
  Sink sink;
  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, kTimeoutMs);
  // As esp_http_client's timeout: no progress for this long fails.
  curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
  curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, kTimeoutMs / 1000);
  curl_easy_setopt(h, CURLOPT_MAXFILESIZE_LARGE, curl_off_t(kBodyCap));
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, onData);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, &sink);
  curl_easy_setopt(h, CURLOPT_USERAGENT, "monster-cyberspace/" RC_VERSION);

  curl_slist* headers = curl_slist_append(nullptr, "Accept: application/json");
  std::string auth;
  if (!bearer.empty()) {
    auth = "Authorization: Bearer " + bearer;
    headers = curl_slist_append(headers, auth.c_str());
  }
  if (jsonBody.empty()) {
    headers = curl_slist_append(headers, "Content-Type:");  // no form type on a bare POST
  } else {
    headers = curl_slist_append(headers, "Content-Type: application/json");
  }
  headers = curl_slist_append(headers, "Expect:");  // no 100-continue round trip
  curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);

  if (strcmp(method, "GET") == 0) {
    curl_easy_setopt(h, CURLOPT_HTTPGET, 1L);
  } else {
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method);
    if (!jsonBody.empty() || strcmp(method, "DELETE") != 0) {
      curl_easy_setopt(h, CURLOPT_POSTFIELDS, jsonBody.data());
      curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(jsonBody.size()));
    }
  }

  const CURLcode err = curl_easy_perform(h);
  curl_slist_free_all(headers);
  if (err == CURLE_FILESIZE_EXCEEDED) sink.overflow = true;
  if (err != CURLE_OK || sink.overflow) {
    r.transportError = true;
    r.overflow = sink.overflow;
    Serial.printf("[http] %s %s failed: %s\n", method, path.c_str(),
                  sink.overflow ? "body too large" : curl_easy_strerror(err));
    close();  // drop a possibly half-read connection
    return r;
  }
  long status = 0;
  curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
  r.status = int(status);
  Serial.printf("[http] %s %s -> %d\n", method, path.c_str(), r.status);
  r.body = std::move(sink.body);
  return r;
}

}  // namespace rc
