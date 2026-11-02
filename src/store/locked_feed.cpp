#include "feature_flags.h"
#if RC_FEAT_ENTRIES

#include "store/locked_feed.h"

namespace rc {

LockedFeed& lockedFeed() {
  static LockedFeed f;
  return f;
}

}  // namespace rc

#endif  // RC_FEAT_ENTRIES
