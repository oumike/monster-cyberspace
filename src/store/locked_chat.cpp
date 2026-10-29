#include "store/locked_chat.h"

namespace rc {

LockedChat& lockedChat() {
  static LockedChat c;
  return c;
}

int cmailUnread() {
  return lockedChat().with([](ChatState& s) {
    int n = 0;
    for (const auto& c : s.conversations) n += c.unread;
    return n;
  });
}

}  // namespace rc
