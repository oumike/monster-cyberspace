#include "chat/typing.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace rc {

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

void mergeTypingJson(TypingEntry& e, JsonObjectConst o) {
  for (JsonPairConst kv : o) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "username")) {
      const char* s = v.as<const char*>();
      e.username = s ? s : "";
    } else if (!strcmp(k, "typing")) {
      e.typing = v.as<bool>();
    } else if (!strcmp(k, "timestamp")) {
      e.timestamp = v.as<int64_t>();
    }
  }
}

std::vector<std::string> typingNames(const TypingMap& map, const std::string& self, int64_t nowMs,
                                     int64_t staleAfterMs) {
  std::vector<std::string> out;
  const std::string me = lower(self);
  for (const auto& kv : map) {
    const TypingEntry& e = kv.second;
    if (!e.typing || e.username.empty() || lower(e.username) == me) continue;
    if (nowMs - e.timestamp > staleAfterMs) continue;
    out.push_back(e.username);
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace rc
