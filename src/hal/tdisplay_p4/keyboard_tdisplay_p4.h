// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/P4Keyboard.{h,cpp} (the hardware
// parts; lib/core's TdisplayP4Keys turns key numbers into actions).
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// The clip-on keyboard expansion: a TCA8418 (0x34, 7 rows x 10 columns) and
// its own XL9555 (0x20: bit 6 the TCA8418's reset, bits 3-5 LEDs lit low) on
// Wire1's keyboard route (i2c1_tdisplay_p4.h); INT on GPIO48, backlight PWM
// on GPIO47. It is fitted with the power off, so it is looked for once.
namespace tdp4::keyboard {

// At boot: true when both the XL9555 and the TCA8418 answer and the matrix is
// set up. Every other call does nothing until it has returned true.
bool detect();
// Drains the controller's event FIFO when INT is low, or now and then anyway.
// Skipped when Wire1 stays busy.
void poll();
// The next drained event: the controller's 1-based key number and whether it
// went down. False when none is waiting.
bool next(uint8_t& keyNum, bool& pressed);
// Backlight duty 0..255; dark while the screen is off.
void light(uint8_t duty, bool screenOn);
// The three LEDs on or off. False when Wire1 stayed busy; try again later.
bool leds(bool on);

}  // namespace tdp4::keyboard

#endif
