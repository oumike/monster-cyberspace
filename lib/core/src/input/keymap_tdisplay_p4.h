#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// A Shift, Sym or Alt press holds for this long, waiting for the key it modifies.
constexpr uint32_t kP4ModifierMs = 1500;
// Held Backspace and arrows repeat after kP4RepeatDelayMs, every kP4RepeatMs.
constexpr uint32_t kP4RepeatDelayMs = 450, kP4RepeatMs = 70;

// The LilyGO T-Display P4's clip-on TCA8418 keyboard, by key number (1-based,
// row*10+col+1, as the controller's event FIFO reports them). Shift (53), Sym
// (51, 58) and Alt (41) are one-shot: a press arms them for the next key, a
// second press disarms, and they lapse after kP4ModifierMs. Caps Lock (31)
// upper-cases letters only; with Shift a letter stays lower case. Esc is Back,
// Enter Select, Tab the palette, Alt+Backspace Back, F11 a light step (read by
// takeLightStep). Layout from wadamesh's P4Keyboard.cpp (kMap).
class TdisplayP4Keys {
 public:
  KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms);
  // A held Backspace's or arrow's repeats; None otherwise.
  KeyEvent poll(uint32_t ms);
  bool capsLock() const { return caps_; }
  // True once after each F11 press.
  bool takeLightStep();

 private:
  uint8_t mods_ = 0;
  uint32_t modMs_ = 0;
  bool caps_ = false;
  bool lightStep_ = false;
  uint8_t heldKey_ = 0;
  Action heldAction_ = Action::None;
  uint32_t nextRepeatMs_ = 0;
};

}  // namespace rc
