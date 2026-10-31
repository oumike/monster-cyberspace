#include "net/sse_client.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "core_version.h"
#include "sys/mem.h"

namespace rc {

namespace {

constexpr int kBufBytes = 4096;
constexpr int kConnectTimeoutMs = 15000;
constexpr int kReadSliceMs = 500;      // how often a blocked read checks stop()
constexpr uint32_t kSilentMs = 75000;  // RTDB sends keep-alive every ~30 s
constexpr int kMaxRedirects = 3;

}  // namespace

SseClient::~SseClient() {
  stop();
  if (done_ && !running_) vSemaphoreDelete(static_cast<SemaphoreHandle_t>(done_));
}

bool SseClient::start(const std::string& url, OnEvent onEvent, OnState onState) {
  if (running_) return false;
  if (!done_) done_ = xSemaphoreCreateBinary();
  if (!done_) return false;
  url_ = url;
  onEvent_ = std::move(onEvent);
  onState_ = std::move(onState);
  parser_.reset();
  stopping_ = false;
  running_ = true;
  if (xTaskCreatePinnedToCore(taskMain, "sse", 8192, this, 4, nullptr, 0) != pdPASS) {
    running_ = false;
    return false;
  }
  return true;
}

void SseClient::stop() {
  if (!running_) return;
  stopping_ = true;
  if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(done_), pdMS_TO_TICKS(2000)) != pdTRUE) {
    Serial.println("[sse] stop: task still closing");
    return;
  }
  while (running_) vTaskDelay(1);
}

void SseClient::taskMain(void* self) {
  static_cast<SseClient*>(self)->run();
  vTaskDelete(nullptr);
}

void SseClient::run() {
  char* buf = static_cast<char*>(rc::mem::big(kBufBytes));
  esp_http_client_handle_t h = nullptr;
  bool live = false;

  if (buf) {
    esp_http_client_config_t cfg = {};
    cfg.url = url_.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = kConnectTimeoutMs;
    cfg.disable_auto_redirect = true;
    cfg.buffer_size = 2048;
    cfg.buffer_size_tx = 2048;  // the URL carries a JWT
    h = esp_http_client_init(&cfg);
  }

  int status = 0;
  if (h) {
    esp_http_client_set_header(h, "Accept", "text/event-stream");
    esp_http_client_set_header(h, "User-Agent", "monster-cyberspace/" RC_VERSION);
    for (int hop = 0; hop <= kMaxRedirects && !stopping_; ++hop) {
      esp_http_client_set_timeout_ms(h, kConnectTimeoutMs);
      if (esp_http_client_open(h, 0) != ESP_OK) {
        status = 0;
        break;
      }
      esp_http_client_fetch_headers(h);
      status = esp_http_client_get_status_code(h);
      // RTDB may move a stream to another server with a 307.
      if (status == 301 || status == 302 || status == 307 || status == 308) {
        esp_http_client_set_redirection(h);
        esp_http_client_close(h);
        continue;
      }
      break;
    }
    Serial.printf("[sse] open -> %d\n", status);
  }

  if (h && status == 200) {
    esp_http_client_set_timeout_ms(h, kReadSliceMs);
    uint32_t lastData = millis();
    while (!stopping_) {
      int n = esp_http_client_read(h, buf, kBufBytes);
      if (n == -ESP_ERR_HTTP_EAGAIN) {
        if (millis() - lastData > kSilentMs) {
          Serial.println("[sse] silent too long");
          break;
        }
        continue;
      }
      if (n <= 0) break;  // closed or failed
      lastData = millis();
      if (!live) {
        live = true;
        if (onState_) onState_(true);
      }
      parser_.feed(buf, size_t(n), [this](const SseEvent& e) {
        if (onEvent_) onEvent_(e);
      });
    }
  }

  if (h) {
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
  }
  free(buf);
  Serial.println("[sse] closed");
  if (onState_) onState_(false);
  // Give before clearing running_, so a start() that sees running_ false
  // can't be followed by this give.
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(done_));
  running_ = false;
}

}  // namespace rc
