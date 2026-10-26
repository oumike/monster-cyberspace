#include "store/locked_feed.h"

namespace rc {

LockedFeed& lockedFeed() {
  static LockedFeed f;
  return f;
}

}  // namespace rc
