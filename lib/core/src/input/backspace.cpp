#include "input/backspace.h"

namespace rc {

BackspaceUse resolveBackspace(bool consumes, uint32_t msSinceLastDelete) {
  if (consumes) return BackspaceUse::Delete;
  return msSinceLastDelete < kBackspaceGuardMs ? BackspaceUse::Drop : BackspaceUse::Back;
}

}  // namespace rc
