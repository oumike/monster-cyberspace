#include "net/https_client.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <cstdlib>
#include <cstring>

#include "core_version.h"

namespace rc {

namespace {

constexpr const char* kBase = "https://api.cyberspace.online";
#if RC_LOW_MEM
// Without PSRAM a body this big couldn't be parsed anyway.
constexpr size_t kBodyCap = 48 * 1024;
#else
constexpr size_t kBodyCap = 256 * 1024;
#endif

// The body is gathered in a malloc'd buffer grown in exact steps: a
// std::string doubles as it grows, which on a board without PSRAM asks for a
// block bigger than any left while the TLS session is open, and its bad_alloc
// can't unwind through the C client. Out of memory counts as too large.
struct Sink {
  char* buf = nullptr;
  size_t size = 0, cap = 0;
  bool overflow = false;
  ~Sink() { free(buf); }
  bool grow(size_t want) {
    if (want <= cap) return true;
    if (want > kBodyCap) return false;
    char* p = static_cast<char*>(realloc(buf, want));
    if (!p) return false;
    buf = p;
    cap = want;
    return true;
  }
};

constexpr size_t kGrowStep = 2048;

esp_err_t onEvent(esp_http_client_event_t* e) {
  if (!e->user_data) return ESP_OK;
  Sink* s = static_cast<Sink*>(e->user_data);
  if (s->overflow) return ESP_OK;
  if (e->event_id == HTTP_EVENT_ON_HEADER && e->header_key && e->header_value &&
      !strcasecmp(e->header_key, "Content-Length")) {
    const long n = strtol(e->header_value, nullptr, 10);
    if (n > 0 && !s->grow(size_t(n))) s->overflow = true;
    return ESP_OK;
  }
  if (e->event_id != HTTP_EVENT_ON_DATA || e->data_len <= 0) return ESP_OK;
  const size_t need = s->size + size_t(e->data_len);
  if (need > s->cap && !s->grow(need + kGrowStep > kBodyCap ? need : need + kGrowStep)) {
    s->overflow = true;
    return ESP_OK;
  }
  memcpy(s->buf + s->size, e->data, size_t(e->data_len));
  s->size = need;
  return ESP_OK;
}

esp_http_client_method_t methodOf(const char* m) {
  if (!strcmp(m, "POST")) return HTTP_METHOD_POST;
  if (!strcmp(m, "DELETE")) return HTTP_METHOD_DELETE;
  if (!strcmp(m, "PATCH")) return HTTP_METHOD_PATCH;
  if (!strcmp(m, "PUT")) return HTTP_METHOD_PUT;
  return HTTP_METHOD_GET;
}

}  // namespace

HttpsClient::~HttpsClient() { close(); }

void HttpsClient::close() {
  if (!handle_) return;
  auto h = static_cast<esp_http_client_handle_t>(handle_);
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
  handle_ = nullptr;
}

HttpResult HttpsClient::request(const char* method, const std::string& path,
                                const std::string& jsonBody, const std::string& bearer) {
  HttpResult r;
  std::string url = std::string(kBase) + path;
  Sink sink;

  if (!handle_) {
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.event_handler = onEvent;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 15000;
    cfg.keep_alive_enable = true;
    cfg.disable_auto_redirect = true;
    cfg.buffer_size = 2048;
    cfg.buffer_size_tx = 2048;  // a bearer JWT is over 1 KB
    handle_ = esp_http_client_init(&cfg);
    if (!handle_) {
      r.transportError = true;
      return r;
    }
  }
  auto h = static_cast<esp_http_client_handle_t>(handle_);
  esp_http_client_set_url(h, url.c_str());
  esp_http_client_set_method(h, methodOf(method));
  esp_http_client_set_user_data(h, &sink);
  esp_http_client_set_header(h, "User-Agent", "monster-cyberspace/" RC_VERSION);
  esp_http_client_set_header(h, "Accept", "application/json");
  if (jsonBody.empty()) {
    esp_http_client_delete_header(h, "Content-Type");
    esp_http_client_set_post_field(h, nullptr, 0);
  } else {
    esp_http_client_set_header(h, "Content-Type", "application/json");
    esp_http_client_set_post_field(h, jsonBody.data(), int(jsonBody.size()));
  }
  if (bearer.empty()) {
    esp_http_client_delete_header(h, "Authorization");
  } else {
    std::string auth = "Bearer " + bearer;
    esp_http_client_set_header(h, "Authorization", auth.c_str());
  }

  esp_err_t err = esp_http_client_perform(h);
  esp_http_client_set_user_data(h, nullptr);
  if (err != ESP_OK || sink.overflow) {
    r.transportError = true;
    r.overflow = sink.overflow;
    r.status = 0;
    Serial.printf("[http] %s %s failed: %s\n", method, path.c_str(),
                  sink.overflow ? "body too large or out of memory" : esp_err_to_name(err));
    close();  // drop a possibly half-read connection
    return r;
  }
  r.status = esp_http_client_get_status_code(h);
  Serial.printf("[http] %s %s -> %d\n", method, path.c_str(), r.status);
#if RC_LOW_MEM
  // The TLS session holds ~50 KB: give it back before the body is copied
  // and parsed.
  close();
#endif
  if (sink.size) r.body.assign(sink.buf, sink.size);
  return r;
}

}  // namespace rc
