#include "features.h"
#if RC_FEAT_PEOPLE

#include "store/locked_people.h"

namespace rc {

LockedPeople& lockedPeople() {
  static LockedPeople p;
  return p;
}

}  // namespace rc

#endif  // RC_FEAT_PEOPLE
