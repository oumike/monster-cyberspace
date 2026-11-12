// SPDX-License-Identifier: GPL-3.0-or-later
#include "feature_flags.h"
#if RC_FEAT_PROGRAMS

#include "store/locked_programs.h"

namespace rc {

LockedPrograms& lockedPrograms() {
  static LockedPrograms p;
  return p;
}

}  // namespace rc

#endif  // RC_FEAT_PROGRAMS
