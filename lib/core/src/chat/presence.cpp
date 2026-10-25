#include "chat/presence.h"

#include <algorithm>
#include <cstring>

namespace rc {

namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

bool nameLess(const PresenceEntry& a, const PresenceEntry& b) {
  const std::string& x = a.username;
  const std::string& y = b.username;
  size_t n = std::min(x.size(), y.size());
  for (size_t i = 0; i < n; ++i) {
    char cx = lower(x[i]), cy = lower(y[i]);
    if (cx != cy) return static_cast<unsigned char>(cx) < static_cast<unsigned char>(cy);
  }
  if (x.size() != y.size()) return x.size() < y.size();
  return a.userId < b.userId;
}

}  // namespace

void mergePresenceJson(PresenceEntry& e, JsonObjectConst o) {
  for (JsonPairConst kv : o) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "userId")) {
      const char* s = v.as<const char*>();
      if (s) e.userId = s;
    } else if (!strcmp(k, "username")) {
      const char* s = v.as<const char*>();
      e.username = s ? s : "";
    } else if (!strcmp(k, "online")) e.online = v.as<bool>();
    else if (!strcmp(k, "isChatAdmin")) e.isChatAdmin = v.as<bool>();
    else if (!strcmp(k, "lastSeen")) e.lastSeen = v.as<int64_t>();
    else if (!strcmp(k, "lastActivity")) e.lastActivity = v.isNull() ? -1 : v.as<int64_t>();
  }
}

std::vector<PresenceEntry> visiblePresence(const PresenceMap& map, int64_t nowMs,
                                           int64_t staleAfterMs) {
  std::vector<PresenceEntry> out;
  for (const auto& kv : map) {
    const PresenceEntry& e = kv.second;
    if (e.online && nowMs - e.lastSeen <= staleAfterMs) out.push_back(e);
  }
  std::sort(out.begin(), out.end(), nameLess);
  return out;
}

bool isIdle(const PresenceEntry& e, int64_t nowMs, int64_t idleAfterMs) {
  if (e.lastActivity == -1) return false;
  return nowMs - e.lastActivity > idleAfterMs;
}

}  // namespace rc
