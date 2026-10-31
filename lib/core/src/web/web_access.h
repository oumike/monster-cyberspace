#pragma once

#include <cstdint>

namespace rc {

// Whether the config page answers a request that arrived on the local
// address `localIp` (host order). The setup AP always serves it, so turning
// web config off can't lock you out; on the joined network it follows the
// "Web config" setting. An `apIp` of 0 means the AP is down.
bool webServes(uint32_t localIp, uint32_t apIp, bool enabled);

}  // namespace rc
