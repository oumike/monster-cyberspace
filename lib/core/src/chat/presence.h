#pragma once

#include <ArduinoJson.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rc {

struct PresenceEntry {
  std::string userId, username;
  bool online = false, isChatAdmin = false;
  int64_t lastSeen = 0, lastActivity = -1;
};
using PresenceMap = std::map<std::string, PresenceEntry>;

// Sets only the keys present. A null lastActivity becomes -1.
void mergePresenceJson(PresenceEntry& e, JsonObjectConst o);

// Online entries seen within staleAfterMs, sorted case-insensitively by name.
std::vector<PresenceEntry> visiblePresence(const PresenceMap& map, int64_t nowMs,
                                           int64_t staleAfterMs);
// False when lastActivity is unknown (-1).
bool isIdle(const PresenceEntry& e, int64_t nowMs, int64_t idleAfterMs);

}  // namespace rc
