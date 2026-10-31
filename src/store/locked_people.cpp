#include "store/locked_people.h"

namespace rc {

LockedPeople& lockedPeople() {
  static LockedPeople p;
  return p;
}

}  // namespace rc
