#pragma once

#include <cstdint>

namespace rc {

// Restarts the device after delayMs, from a short background task so the
// caller can return (e.g. to let an HTTP response go out first).
void restartSoon(uint32_t delayMs);

}  // namespace rc
