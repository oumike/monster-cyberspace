#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// The Xteink X4's buttons, as bits of the mask x4ButtonMask returns.
constexpr uint8_t kX4Back = 1 << 0, kX4Confirm = 1 << 1, kX4Left = 1 << 2, kX4Right = 1 << 3,
                  kX4Up = 1 << 4, kX4Down = 1 << 5, kX4Power = 1 << 6;

// A reading must hold this long to count (press or release).
constexpr uint32_t kX4DebounceMs = 50;
// Confirm and Back held this long: the palette and Home.
constexpr uint32_t kX4HoldMs = 700;
// Power held this long: power off.
constexpr uint32_t kX4PowerHoldMs = 2000;
// Held arrows repeat after kX4RepeatDelayMs, every kX4RepeatMs (slow: each
// step may refresh the e-paper).
constexpr uint32_t kX4RepeatDelayMs = 600, kX4RepeatMs = 300;

// The two resistor ladders (rhino-moshi's x4.h), in 16-bit ADC units (the
// 12-bit reading shifted left by 4). Ladder 1: Back, Confirm, Left, Right;
// ladder 2: Up, Down. A reading is button i when ranges[i+1] < raw <=
// ranges[i]; above ranges[0] is idle. powerDown: GPIO3 reads low.
uint8_t x4ButtonMask(int adc1, int adc2, bool powerDown);

// Debounced buttons to keys. Arrows on press, repeating while held. Confirm:
// tap Select, hold the palette. Back: tap Back, hold Home. Power: tap
// ScreenOff, hold PowerOff. Taps are sent on release, holds once when due.
class X4Keys {
 public:
  // Feeds one sample (every few ms); writes up to max events, returns how many.
  int update(uint8_t raw, uint32_t ms, KeyEvent* out, int max);

 private:
  uint8_t raw_ = 0, down_ = 0, heldFired_ = 0;
  uint32_t rawMs_[7] = {}, pressMs_[7] = {}, nextMs_[7] = {};
};

}  // namespace rc
