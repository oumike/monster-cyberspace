#include "web/web_access.h"

namespace rc {

bool webServes(uint32_t localIp, uint32_t apIp, bool enabled) {
  if (enabled) return true;
  return apIp != 0 && localIp == apIp;
}

}  // namespace rc
