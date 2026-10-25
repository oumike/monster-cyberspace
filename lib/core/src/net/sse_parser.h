#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace rc {

struct SseEvent {
  std::string event;
  std::string data;
};

// Incremental text/event-stream parser. Accepts input split at any byte.
// Lines end with "\n", "\r\n" or "\r". An event whose buffered line + data
// exceeds maxEventBytes is discarded; overflowed() then stays true until the
// next event is dispatched.
class SseParser {
 public:
  using Handler = std::function<void(const SseEvent&)>;

  void feed(const char* buf, size_t len, const Handler& onEvent);
  void reset();
  bool overflowed() const { return overflowed_; }

  size_t maxEventBytes = 256 * 1024;

 private:
  void endLine(const Handler& onEvent);

  std::string line_;
  std::string event_;
  std::string data_;
  bool hasData_ = false;
  bool afterCR_ = false;
  bool discarding_ = false;
  size_t discardLineLen_ = 0;
  bool overflowed_ = false;
};

}  // namespace rc
