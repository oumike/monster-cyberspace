#pragma once

#include <ArduinoJson.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rc {

// One participant's entry in dm_presence/<conversationId>, keyed by userId.
struct TypingEntry {
  std::string userId, username;
  bool typing = false;
  int64_t timestamp = 0;
};
using TypingMap = std::map<std::string, TypingEntry>;

// From POST /v1/cmail/:id/typing. Defaults match the API docs.
struct TypingCadence {
  int64_t heartbeatMs = 3000, staleAfterMs = 9000;
};

// Sets only the keys present.
void mergeTypingJson(TypingEntry& e, JsonObjectConst o);

// Usernames typing now: typing is true and the timestamp is within
// staleAfterMs. `self` (any case) is left out.
std::vector<std::string> typingNames(const TypingMap& map, const std::string& self, int64_t nowMs,
                                     int64_t staleAfterMs);

}  // namespace rc
