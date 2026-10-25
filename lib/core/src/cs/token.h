#pragma once

#include <cstdint>
#include <string>

namespace rc {

// Reads `exp` (seconds) from a JWT's base64url payload. -1 on any failure.
// The signature is not verified; this only schedules a refresh.
int64_t jwtExpSeconds(const std::string& jwt);

// Monotonic time at which to refresh a token received at receivedMonoMs:
// five minutes before exp (at least 60 s out) when the clock is synced
// (nowEpochSec >= 1700000000) and exp is known, else receipt + 55 min.
int64_t refreshDueMonoMs(int64_t receivedMonoMs, int64_t nowEpochSec, int64_t expSec);

}  // namespace rc
