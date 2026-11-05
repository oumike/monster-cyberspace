#pragma once

#include <cstdint>

namespace rc {

// A Li-ion cell's voltage as a rough charge level, shared by every board.
// < 2.5 V → -1 (no reading); else (v - 3.3) / (4.15 - 3.3) * 100, rounded,
// clamped 0..100.
int batteryPercentFromVolts(float v);

// The Tanmatsu's coprocessor reads the cell under the P4 and panel's load, so
// it uses badge-bsp's curve (4.05 V full, 3.30 V empty), interpolated. Under
// 2.5 V → -1 (no reading).
int tanmatsuBatteryPercent(float v);

// Its two VBAT bytes are rewritten while being read, so one read can pair
// bytes of two samples. Two reads count only when both are in the PMIC ADC's
// 2304..4848 mV range and agree within 40 mV; mv is then their mean.
bool tanmatsuVbatAgree(uint16_t a, uint16_t b, uint16_t& mv);

}  // namespace rc
