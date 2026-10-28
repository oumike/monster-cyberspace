#include "app/ota_service.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "app/app_state.h"
#include "app/ota_signing_pubkey.h"
#include "app/restart.h"
#include "core_version.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "ota/ota_logic.h"

// Arduino marks a freshly installed app valid at boot unless this says
// otherwise; the task below does it after kValidAfterMs instead, so an update
// that crashes early is rolled back by the bootloader.
extern "C" bool verifyRollbackLater() { return true; }

namespace ota {

namespace {

constexpr int kTimeoutMs = 20000;
constexpr uint32_t kFirstCheckDelayMs = 20000;  // after joining Wi-Fi
constexpr uint32_t kCheckIntervalMs = 24UL * 3600UL * 1000UL;
constexpr uint32_t kValidAfterMs = 30000;
constexpr int kMaxRedirects = 5;
constexpr const char* kUserAgent = "monster-cyberspace-ota/" RC_VERSION;

SemaphoreHandle_t gMtx = nullptr;
Status gSt;
volatile bool gBusy = false;
bool gAutoTried = false;  // one automatic install per release found

template <class F>
void locked(F f) {
  xSemaphoreTake(gMtx, portMAX_DELAY);
  f();
  xSemaphoreGive(gMtx);
}

void setState(State st, const std::string& err = {}) {
  locked([&] {
    gSt.state = st;
    gSt.error = err;
  });
}

esp_http_client_handle_t newClient(const char* url, bool followRedirects) {
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = kTimeoutMs;
  cfg.disable_auto_redirect = !followRedirects;
  cfg.max_redirection_count = kMaxRedirects;
  // GitHub's redirects to its asset host carry long signed URLs and headers.
  cfg.buffer_size = 4096;
  cfg.buffer_size_tx = 4096;
  cfg.user_agent = kUserAgent;
  return esp_http_client_init(&cfg);
}

// Opens url for reading, following redirects by hand (the streaming API does
// not). Returns the status code, or -1 on a transport error.
int openFollowing(esp_http_client_handle_t h, int64_t& length) {
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    if (esp_http_client_open(h, 0) != ESP_OK) return -1;
    length = esp_http_client_fetch_headers(h);
    const int code = esp_http_client_get_status_code(h);
    if (code < 300 || code >= 400) return code;
    if (esp_http_client_set_redirection(h) != ESP_OK) return code;
    esp_http_client_close(h);
  }
  return -1;
}

std::string httpErr(const char* what, int code) {
  return code < 0 ? std::string(what) + ": network error" : std::string(what) + ": HTTP " + std::to_string(code);
}

// /releases/latest answers 302 to /releases/tag/<tag>; the tag is read from
// that Location without following it.
std::string gLocation;
esp_err_t onLatestEvent(esp_http_client_event_t* e) {
  if (e->event_id == HTTP_EVENT_ON_HEADER && e->header_key && e->header_value &&
      !strcasecmp(e->header_key, "Location"))
    gLocation = e->header_value;
  return ESP_OK;
}

bool fetchLatestTag(std::string& tag, std::string& err) {
  esp_http_client_config_t cfg = {};
  cfg.url = rc::kOtaLatestUrl;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = kTimeoutMs;
  cfg.disable_auto_redirect = true;
  cfg.buffer_size = 4096;
  cfg.buffer_size_tx = 2048;
  cfg.user_agent = kUserAgent;
  cfg.event_handler = onLatestEvent;
  cfg.method = HTTP_METHOD_HEAD;
  esp_http_client_handle_t h = esp_http_client_init(&cfg);
  if (!h) {
    err = "no memory";
    return false;
  }
  gLocation.clear();
  const esp_err_t e = esp_http_client_perform(h);
  const int code = e == ESP_OK ? esp_http_client_get_status_code(h) : -1;
  esp_http_client_cleanup(h);
  if (code < 300 || code >= 400) {
    err = httpErr("check", code);
    return false;
  }
  tag = rc::tagFromLocation(gLocation);
  if (tag.empty()) {
    err = "no release yet";
    return false;
  }
  return true;
}

bool fetchSignature(const std::string& url, uint8_t* buf, size_t cap, size_t& len, std::string& err) {
  esp_http_client_handle_t h = newClient(url.c_str(), false);
  if (!h) {
    err = "no memory";
    return false;
  }
  int64_t total = 0;
  const int code = openFollowing(h, total);
  len = 0;
  if (code == 200 && total <= int64_t(cap)) {
    while (len < cap) {
      const int n = esp_http_client_read(h, reinterpret_cast<char*>(buf) + len, int(cap - len));
      if (n <= 0) break;
      len += size_t(n);
    }
  }
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
  if (code != 200) err = httpErr("signature", code);
  else if (total > int64_t(cap)) err = "signature too large";
  else if (!len) err = "empty signature";
  return code == 200 && total <= int64_t(cap) && len > 0;
}

// Detached ECDSA P-256 (DER) over the SHA-256 of the image. Fails closed.
bool verifySignature(const uint8_t* hash, const uint8_t* sig, size_t sigLen) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int rc = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char*>(OTA_SIGNING_PUBKEY_PEM),
                                       strlen(OTA_SIGNING_PUBKEY_PEM) + 1);
  if (rc == 0) rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, sigLen);
  mbedtls_pk_free(&pk);
  if (rc != 0) Serial.printf("[ota] signature check failed: -0x%04x\n", unsigned(-rc));
  return rc == 0;
}

// The dual-slot table this project ships: ota_0 at 0x10000, an ota_1 of the
// same size, no ota_2, and the running app in one of them. Anything else gets
// no update: the spare slot may hold another firmware.
bool layoutSupportsUpdate() {
  auto find = [](esp_partition_subtype_t t) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, t, nullptr);
  };
  const esp_partition_t* s0 = find(ESP_PARTITION_SUBTYPE_APP_OTA_0);
  const esp_partition_t* s1 = find(ESP_PARTITION_SUBTYPE_APP_OTA_1);
  if (!s0 || !s1 || find(ESP_PARTITION_SUBTYPE_APP_OTA_2) || s0->size != s1->size || s0->address != 0x10000)
    return false;
  const esp_partition_t* run = esp_ota_get_running_partition();
  return run && (run->address == s0->address || run->address == s1->address);
}

void doCheck() {
  setState(State::Checking);
  std::string tag, err;
  if (!fetchLatestTag(tag, err)) {
    Serial.printf("[ota] check failed: %s\n", err.c_str());
    setState(State::Failed, err);
    return;
  }
  const bool newer = rc::compareVersions(RC_VERSION, tag.c_str()) < 0;
  locked([&] {
    if (gSt.latest != tag) gAutoTried = false;
    gSt.latest = tag;
  });
  Serial.printf("[ota] latest %s, running %s%s\n", tag.c_str(), RC_VERSION, newer ? ": update available" : "");
  setState(newer ? State::Available : State::UpToDate);
}

bool doInstall(std::string& err) {
  std::string tag;
  locked([&] { tag = gSt.latest; });
  if (tag.empty() && !fetchLatestTag(tag, err)) return false;
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next) {
    err = "no update slot";
    return false;
  }
  const std::string url = rc::otaAssetUrl(board::otaSlug(), tag);

  // Signature first: a missing one stops before the slot is touched.
  uint8_t sig[128];
  size_t sigLen = 0;
  if (!fetchSignature(url + ".sig", sig, sizeof sig, sigLen, err)) return false;

  esp_http_client_handle_t h = newClient(url.c_str(), false);
  if (!h) {
    err = "no memory";
    return false;
  }
  int64_t len = 0;
  const int code = openFollowing(h, len);
  if (code != 200) {
    err = httpErr("download", code);
    esp_http_client_cleanup(h);
    return false;
  }
  if (len > int64_t(next->size)) {
    err = "image too large: install over USB";
    esp_http_client_cleanup(h);
    return false;
  }
  esp_ota_handle_t ota = 0;
  if (esp_ota_begin(next, len > 0 ? size_t(len) : OTA_SIZE_UNKNOWN, &ota) != ESP_OK) {
    err = "could not erase the update slot";
    esp_http_client_cleanup(h);
    return false;
  }
  locked([&] {
    gSt.written = 0;
    gSt.total = len > 0 ? size_t(len) : 0;
  });

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  static char chunk[4096];
  size_t written = 0;
  bool ok = true;
  while (len <= 0 || written < size_t(len)) {
    const int n = esp_http_client_read(h, chunk, sizeof chunk);
    if (n < 0) {
      err = "download failed";
      ok = false;
      break;
    }
    if (n == 0) {
      if (esp_http_client_is_complete_data_received(h)) break;
      err = "download stalled";
      ok = false;
      break;
    }
    if (esp_ota_write(ota, chunk, size_t(n)) != ESP_OK) {
      err = "flash write failed";
      ok = false;
      break;
    }
    mbedtls_sha256_update(&sha, reinterpret_cast<const unsigned char*>(chunk), size_t(n));
    written += size_t(n);
    locked([&] { gSt.written = written; });
  }
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (ok && len > 0 && written != size_t(len)) {
    err = "download incomplete";
    ok = false;
  }
  // Checked BEFORE the slot is made bootable: a tampered or corrupt image
  // fails here and is never booted.
  if (ok && !verifySignature(digest, sig, sigLen)) {
    err = "signature check failed";
    ok = false;
  }
  if (!ok) {
    esp_ota_abort(ota);
    return false;
  }
  if (esp_ota_end(ota) != ESP_OK) {
    err = "image invalid";
    return false;
  }
  if (esp_ota_set_boot_partition(next) != ESP_OK) {
    err = "could not select the new slot";
    return false;
  }
  Serial.printf("[ota] installed %s (%u bytes)\n", tag.c_str(), unsigned(written));
  return true;
}

void worker(void* arg) {
  if (arg) {
    setState(State::Installing);
    std::string err;
    if (doInstall(err)) {
      setState(State::Done);
      rc::restartSoon(2000);
    } else {
      Serial.printf("[ota] install failed: %s\n", err.c_str());
      setState(State::Failed, err);
    }
  } else {
    doCheck();
  }
  gBusy = false;
  vTaskDelete(nullptr);
}

bool begin(bool install) {
  if (!gMtx || !wifimgr::connected()) return false;
  bool blocked = false;
  locked([&] {
    blocked = gBusy || !gSt.layoutOk || gSt.state == State::Done || (install && gSt.latest.empty());
    if (!blocked) gBusy = true;  // the UI, the web server and the scheduler all start jobs
  });
  if (blocked) return false;
  if (xTaskCreatePinnedToCore(worker, install ? "ota-inst" : "ota-chk", 12288, install ? (void*)1 : nullptr, 1,
                              nullptr, 0) != pdPASS) {
    gBusy = false;
    return false;
  }
  return true;
}

void scheduler(void*) {
  const uint32_t bootMs = millis();
  bool validMarked = false, wasConnected = false;
  uint32_t nextCheckMs = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    const uint32_t now = millis();
    if (!validMarked && now - bootMs >= kValidAfterMs) {
      validMarked = true;
      esp_ota_img_states_t st;
      if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
          st == ESP_OTA_IMG_PENDING_VERIFY)
        Serial.println("[ota] new firmware is running fine; keeping it");
      esp_ota_mark_app_valid_cancel_rollback();
    }
    const bool connected = wifimgr::connected();
    if (connected && !wasConnected) nextCheckMs = now + kFirstCheckDelayMs;
    wasConnected = connected;
    if (!connected || gBusy) continue;
    bool autoGo = false;
    locked([&] {
      if (rc::app().cfg.autoUpdate && !gAutoTried && gSt.state == State::Available) {
        gAutoTried = true;
        autoGo = true;
      }
    });
    if (autoGo) {
      Serial.println("[ota] installing automatically");
      begin(true);
      continue;
    }
    if (!nextCheckMs || int32_t(now - nextCheckMs) < 0) continue;
    nextCheckMs = now + kCheckIntervalMs;
    begin(false);
  }
}

}  // namespace

void start() {
  gMtx = xSemaphoreCreateMutex();
  gSt.layoutOk = layoutSupportsUpdate();
  if (!gSt.layoutOk) Serial.println("[ota] flash layout not ours: updates disabled");
  xTaskCreatePinnedToCore(scheduler, "ota", 4096, nullptr, 1, nullptr, 0);
}

Status status() {
  Status st;
  if (gMtx) locked([&] { st = gSt; });
  return st;
}

const char* stateName(State s) {
  switch (s) {
    case State::Idle: return "idle";
    case State::Checking: return "checking";
    case State::UpToDate: return "up-to-date";
    case State::Available: return "available";
    case State::Installing: return "installing";
    case State::Done: return "done";
    case State::Failed: return "failed";
  }
  return "?";
}

bool startCheck() { return begin(false); }
bool startInstall() { return begin(true); }

int percent(const Status& s) {
  if (!s.total) return -1;
  return int(uint64_t(s.written) * 100 / s.total);
}

}  // namespace ota
