// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// The Tab5 Keyboard: an STM32F030 at 0x6D on Wire1 (Ext.Port1), run in its
// Normal mode, where every press and release is one event byte (bit 7
// pressed, bits 6-4 row, bits 3-0 column; rc::Tab5Keys reads them). Two RGB
// LEDs. It is fitted with the power off, so it is looked for once.
namespace tab5::keyboard {

// At boot: true when the keyboard answers and is set to Normal mode with the
// LEDs under our control.
bool detect();
// The next event byte, read while INT is low (and now and then anyway).
// False when none is waiting.
bool next(uint8_t& event);
// LED i (0 or 1) to rgb bits (bit 0 red, 1 green, 2 blue) at level 0..255.
void led(int i, uint8_t rgb, uint8_t level);

}  // namespace tab5::keyboard

#endif
