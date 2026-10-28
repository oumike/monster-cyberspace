#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// A Shift or Sym press holds for this long, waiting for the key it modifies.
constexpr uint32_t kPagerModifierMs = 1500;
// Held Backspace repeats after kPagerRepeatDelayMs, every kPagerRepeatMs.
constexpr uint32_t kPagerRepeatDelayMs = 500, kPagerRepeatMs = 70;

// The LilyGO T-LoRa Pager's TCA8418 keyboard, by key number (1-based, as the
// controller's event FIFO reports them). Shift (29) and Sym (21) are one-shot:
// a press arms them for the next key, a second press disarms. Every Sym press
// also sends Symbols (modifier set): while typing it opens the symbol table,
// and in the table the emoji tray. The next key still gets the Sym layer. Enter is Select, Sym+Enter the palette,
// Sym+Backspace Home. Layout from camillia-mt's
// keyboard.cpp (kTloraTapMap).
class PagerKeys {
 public:
  KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms);
  // A held Backspace's repeats; None otherwise.
  KeyEvent poll(uint32_t ms);

 private:
  uint8_t mods_ = 0;
  uint32_t modMs_ = 0;
  bool bsHeld_ = false;
  uint32_t bsNextMs_ = 0;
};

}  // namespace rc
