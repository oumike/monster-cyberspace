// SPDX-License-Identifier: GPL-3.0-or-later
// SseClient on libcurl, on its own thread, feeding the same SseParser.
// stop() aborts the transfer through the progress callback (spec section
// 4.2). The URL carries the auth token and is never logged.
#include "net/sse_client.h"

#include <Arduino.h>
#include <curl/curl.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "core_version.h"

namespace rc {

namespace {

constexpr long kConnectTimeoutMs = 15000;
constexpr long kSilentSec = 75;  // RTDB sends keep-alive every ~30 s
constexpr long kMaxRedirects = 3;

// What done_ points at: set when the thread has ended.
struct Done {
  std::mutex mu;
  std::condition_variable cv;
  bool done = true;
};

struct Transfer {
  SseClient* self;
  SseParser* parser;
  const SseClient::OnEvent* onEvent;
  const SseClient::OnState* onState;
  const std::atomic<bool>* stopping;
  CURL* h;
  bool checked = false;  // the status was looked at
  bool live = false;
};

size_t onData(char* p, size_t size, size_t n, void* user) {
  auto* t = static_cast<Transfer*>(user);
  const size_t len = size * n;
  if (*t->stopping) return 0;
  if (!t->checked) {
    t->checked = true;
    long status = 0;
    curl_easy_getinfo(t->h, CURLINFO_RESPONSE_CODE, &status);
    Serial.printf("[sse] open -> %ld\n", status);
    if (status != 200) return 0;
  }
  if (!t->live) {
    t->live = true;
    if (*t->onState) (*t->onState)(true);
  }
  t->parser->feed(p, len, [t](const SseEvent& e) {
    if (*t->onEvent) (*t->onEvent)(e);
  });
  return len;
}

// Called at least once a second while the transfer runs, even when idle.
int onProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  return *static_cast<Transfer*>(user)->stopping ? 1 : 0;
}

void curlGlobalInit() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

}  // namespace

SseClient::~SseClient() {
  stop();
  auto* d = static_cast<Done*>(done_);
  if (!d) return;
  // Unlike a FreeRTOS task, the thread can't outlive this object: wait it out.
  std::unique_lock<std::mutex> lk(d->mu);
  d->cv.wait(lk, [d] { return d->done; });
  lk.unlock();
  while (running_) std::this_thread::yield();
  delete d;
}

bool SseClient::start(const std::string& url, OnEvent onEvent, OnState onState) {
  if (running_) return false;
  if (!done_) done_ = new Done;
  auto* d = static_cast<Done*>(done_);
  curlGlobalInit();
  url_ = url;
  onEvent_ = std::move(onEvent);
  onState_ = std::move(onState);
  parser_.reset();
  stopping_ = false;
  {
    std::lock_guard<std::mutex> g(d->mu);
    d->done = false;
  }
  running_ = true;
  try {
    std::thread(taskMain, this).detach();
  } catch (...) {
    std::lock_guard<std::mutex> g(d->mu);
    d->done = true;
    running_ = false;
    return false;
  }
  return true;
}

void SseClient::stop() {
  if (!running_) return;
  stopping_ = true;
  auto* d = static_cast<Done*>(done_);
  std::unique_lock<std::mutex> lk(d->mu);
  if (!d->cv.wait_for(lk, std::chrono::seconds(2), [d] { return d->done; })) {
    Serial.println("[sse] stop: task still closing");
    return;
  }
  lk.unlock();
  while (running_) std::this_thread::yield();
}

void SseClient::taskMain(void* self) { static_cast<SseClient*>(self)->run(); }

void SseClient::run() {
  CURL* h = curl_easy_init();
  Transfer t{this, &parser_, &onEvent_, &onState_, &stopping_, h};
  if (h) {
    curl_slist* headers = curl_slist_append(nullptr, "Accept: text/event-stream");
    curl_easy_setopt(h, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "monster-cyberspace/" RC_VERSION);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    // RTDB may move a stream to another server with a 307.
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, kMaxRedirects);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, kSilentSec);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, onData);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &t);
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(h, CURLOPT_XFERINFODATA, &t);
    const CURLcode err = curl_easy_perform(h);
    if (!t.checked && !stopping_) {
      long status = 0;
      curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
      Serial.printf("[sse] open -> %ld\n", err == CURLE_OK ? status : 0L);
    } else if (err == CURLE_OPERATION_TIMEDOUT && t.live) {
      Serial.println("[sse] silent too long");
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(h);
  }
  Serial.println("[sse] closed");
  if (onState_) onState_(false);
  auto* d = static_cast<Done*>(done_);
  {
    std::lock_guard<std::mutex> g(d->mu);
    d->done = true;
  }
  d->cv.notify_all();
  running_ = false;
}

}  // namespace rc
