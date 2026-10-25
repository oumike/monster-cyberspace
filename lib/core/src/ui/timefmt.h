#pragma once

#include <cstdint>
#include <string>

namespace rc {

// Local time (process TZ) as "14:02".
std::string formatHHMM(int64_t epochMs);
// Local date as "Sat 25 Oct" (day without a leading zero).
std::string formatDay(int64_t epochMs);
// Days since the epoch in local time, for detecting day changes.
int64_t localDayNumber(int64_t epochMs);
// Minutes since the epoch in local time, for detecting minute changes.
int64_t localMinuteNumber(int64_t epochMs);

}  // namespace rc
