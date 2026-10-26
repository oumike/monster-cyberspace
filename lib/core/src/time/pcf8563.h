#pragma once

#include <cstdint>

// The PCF8563 RTC's time registers 0x02..0x08 (seconds ... years), in BCD.
// Weekday 0 is Sunday.
namespace rc {

// Years 2000-2099; the century bit stays 0.
void pcf8563Encode(int64_t epochSec, uint8_t regs[7]);
// False when the VL (voltage low) bit of regs[0] is set or a field is out of range.
bool pcf8563Decode(const uint8_t regs[7], int64_t& epochSec);

}  // namespace rc
