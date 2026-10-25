#include "net/https_client.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <cstring>

#include "core_version.h"

namespace rc {

namespace {

constexpr const char* kBase = "https://api.cyberspace.online";
constexpr size_t kBodyCap = 256 * 1024;

struct Sink {
  std::string* body;
  bool overflow = false;
};

esp_err_t onEvent(esp_http_client_event_t* e) {
  if (e->event_id != HTTP_EVENT_ON_DATA || !e->user_data) return ESP_OK;
  Sink* s = static_cast<Sink*>(e->user_data);
  if (s->body->size() + e->data_len > kBodyCap) {
    s->overflow = true;
    return ESP_OK;
  }
  s->body->append(static_cast<const char*>(e->data), e->data_len);
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
  Sink sink{&r.body};

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
  esp_http_client_set_header(h, "User-Agent", "rhino-cyberspace/" RC_VERSION);
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
    r.status = 0;
    Serial.printf("[http] %s %s failed: %s\n", method, path.c_str(),
                  sink.overflow ? "body too large" : esp_err_to_name(err));
    close();  // drop a possibly half-read connection
    return r;
  }
  r.status = esp_http_client_get_status_code(h);
  Serial.printf("[http] %s %s -> %d\n", method, path.c_str(), r.status);
  return r;
}

}  // namespace rc
