// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/Hi8561Touch.cpp (its GT9895 path).
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// The AMOLED's Goodix GT9895 at 0x5D on Wire (SDA 7 / SCL 8), polled over
// I2C: its reset is XL9535 bit 3, and its INT (bit 4) is not used.
namespace tdp4::touch {

// Resets it through the XL9535 and probes it. False when it is absent.
bool begin();
// One finger in the raw 1060 x 2400 grid, x along the panel's 568 side;
// false when no finger is down, or the controller is absent.
bool read(int16_t& rx, int16_t& ry);

}  // namespace tdp4::touch

#endif
