#pragma once

#include <cstdint>

namespace rc {

// Raw touch in the controller's native portrait (nativeW x nativeH) to frame
// coordinates for rotation r, in quarter turns clockwise from the board's
// landscape: 0 Landscape, 1 Portrait, 2 Landscape 180, 3 Portrait 180 (r mod 4).
// Raw values outside the native size clamp to the edge first.
void touchToFrame(int16_t rx, int16_t ry, int nativeW, int nativeH, uint8_t r,
                  int16_t& x, int16_t& y);

}  // namespace rc
