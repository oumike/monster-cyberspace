#include "chat/mention.h"

namespace rc {

namespace {

bool isWord(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

char lower(char c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

}  // namespace

bool mentionsUser(std::string_view content, std::string_view username) {
  if (username.empty()) return false;
  const size_t n = username.size();
  for (size_t at = content.find('@'); at != std::string_view::npos;
       at = content.find('@', at + 1)) {
    if (at > 0 && isWord(content[at - 1])) continue;
    if (content.size() - at - 1 < n) break;
    bool match = true;
    for (size_t k = 0; k < n && match; ++k)
      match = lower(content[at + 1 + k]) == lower(username[k]);
    if (!match) continue;
    size_t after = at + 1 + n;
    if (after == content.size() || !isWord(content[after])) return true;
  }
  return false;
}

}  // namespace rc
