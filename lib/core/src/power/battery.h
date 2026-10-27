#pragma once

namespace rc {

// A Li-ion cell's voltage as a rough charge level, shared by every board.
// < 2.5 V → -1 (no reading); else (v - 3.3) / (4.15 - 3.3) * 100, rounded,
// clamped 0..100.
int batteryPercentFromVolts(float v);

}  // namespace rc
