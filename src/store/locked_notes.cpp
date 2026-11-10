#include "feature_flags.h"
#if RC_FEAT_NOTES

#include "store/locked_notes.h"

namespace rc {

LockedNotes& lockedNotes() {
  static LockedNotes n;
  return n;
}

}  // namespace rc

#endif  // RC_FEAT_NOTES
