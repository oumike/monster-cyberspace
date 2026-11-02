// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/P4I2c1.{h,cpp}.
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

class TwoWire;

// I2C controller 1 (Wire1) serves two pin pairs: the keyboard expansion on
// SDA 46 / SCL 45 and the codec and haptic driver on 20/21. Controller 0 is
// the main board's bus, so the two take turns on controller 1. Every Wire1
// transaction sits between acquire and release.
namespace tdp4::i2c1 {

enum Route : uint8_t { Keyboard, Audio };

// Takes the bus, points Wire1 at the route (re-pinning it when it is on the
// other one or not running) and returns it. nullptr when the bus stays busy
// for longer than waitMs, or would not start; then there is nothing to release.
TwoWire* acquire(Route route, uint32_t waitMs);
void release();

}  // namespace tdp4::i2c1

#endif
