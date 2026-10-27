#pragma once

#include <cstdint>

namespace rc {

// Panel rotation r, in quarter turns clockwise from the board's landscape:
// 0 Landscape, 1 Portrait, 2 Landscape 180, 3 Portrait 180.
constexpr uint8_t kRotationOrder[4] = {0, 2, 1, 3};  // the Settings / web order
const char* rotationLabel(uint8_t r);  // "Landscape", "Portrait", "Landscape 180", "Portrait 180"
uint8_t rotationFromIndex(int i);      // out of 0..3 → 0
bool rotationPortrait(uint8_t r);      // r odd

}  // namespace rc
