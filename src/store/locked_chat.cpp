#include "store/locked_chat.h"

namespace rc {

LockedChat& lockedChat() {
  static LockedChat c;
  return c;
}

}  // namespace rc
