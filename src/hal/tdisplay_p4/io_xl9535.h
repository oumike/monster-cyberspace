// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/Xl9535.{h,cpp}.
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// The base board's XL9535 expander (0x20 on Wire, SDA 7 / SCL 8), which gates
// the power rails and the C6, SD, panel, touch and radio lines. Bits are as in
// pins_tdisplay_p4.h (0-15). The caller has begun Wire.
namespace tdp4::xl9535 {

// Probes it, then drives every pin as an output, high. False when it is absent.
bool begin();
// wadamesh's tested order: the inputs, the rails power-cycled (VCCA low; 5V
// high, low, high; 3V3 low, high, low; 200 ms each), SD powered, radio reset
// parked high, the on-board antenna, touch released, then the C6 enable
// pulsed high, low, high (100 ms each). The screen reset is the panel's.
void powerOnSequence();
void set(uint8_t bit, bool high);
bool get(uint8_t bit);

}  // namespace tdp4::xl9535

#endif
