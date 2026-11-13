// SPDX-License-Identifier: GPL-3.0-or-later
#include "feature_flags.h"
#if RC_FEAT_SEARCH

#include "store/locked_search.h"

namespace rc {

LockedSearch& lockedSearch() {
  static LockedSearch s;
  return s;
}

}  // namespace rc

#endif  // RC_FEAT_SEARCH
