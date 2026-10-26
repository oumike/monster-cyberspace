#pragma once

#include <cstdint>

// Calendar dates in the proleptic Gregorian calendar, UTC.
namespace rc {

// Days since 1970-01-01 (which is 0).
int64_t daysFromCivil(int64_t y, int m, int d);
void civilFromDays(int64_t days, int& y, int& m, int& d);

}  // namespace rc
