#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "net/sse_parser.h"

namespace rc {

// One Server-Sent Events stream on its own task (core 0). The callbacks run on
// that task. It never reconnects by itself: the owner reopens with a fresh
// token and startAfter. The URL carries the auth token and is never logged.
class SseClient {
 public:
  using OnEvent = std::function<void(const SseEvent&)>;
  using OnState = std::function<void(bool live)>;

  ~SseClient();

  // False when a previous stream is still shutting down or the task can't
  // start.
  bool start(const std::string& url, OnEvent onEvent, OnState onState);
  // Closes the stream and waits up to 2 s for the task to end.
  void stop();
  bool running() const { return running_; }

 private:
  static void taskMain(void* self);
  void run();

  std::string url_;
  OnEvent onEvent_;
  OnState onState_;
  SseParser parser_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopping_{false};
  void* done_ = nullptr;  // SemaphoreHandle_t, given when the task ends
};

}  // namespace rc
