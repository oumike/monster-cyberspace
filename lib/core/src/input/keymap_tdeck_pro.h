#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// A Shift, Sym or Alt tap holds for this long, waiting for the key it modifies.
constexpr uint32_t kTdeckProModifierMs = 1500;
// Held Backspace and held Alt arrows repeat after kTdeckProRepeatDelayMs,
// every kTdeckProRepeatMs.
constexpr uint32_t kTdeckProRepeatDelayMs = 500, kTdeckProRepeatMs = 120;

// The LilyGO T-Deck Pro's TCA8418 keyboard, by key number (1-based, as the
// controller's event FIFO reports them; the matrix starts at the top right, so
// 1 is 'p'). Layout from platy-aprs's tdeck_pro_io.cpp (camillia-mt's map).
//
// Shift (31, 35) and Sym (32) are one-shot: a tap arms them for the next key,
// a second tap disarms. Every Sym press also sends Symbols (modifier set): while
// typing it opens the symbol table, and in the table the emoji tray. Alt (30)
// works held, for every key while it is down, or as a one-shot tap.
//
// Enter is Select, Backspace Backspace. Sym+Enter is the palette, Sym+Backspace
// Back and Sym+Space Home. The board has no arrows, so Alt chords move:
// Alt+W/A/S/D Up/Left/Down/Right, Alt+Backspace Back, Alt+Enter the palette,
// Alt+H Home, Alt+C cIRC, Alt+M C-Mail, Alt+F Feed.
class TdeckProKeys {
 public:
  KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms);
  // A held Backspace's or Alt arrow's repeats; None otherwise.
  KeyEvent poll(uint32_t ms);

 private:
  uint8_t mods_ = 0;
  uint32_t modMs_ = 0;
  bool altDown_ = false;
  uint8_t heldKey_ = 0;  // the key repeating, 0 for none
  Action heldAction_ = Action::None;
  uint32_t heldNextMs_ = 0;
};

}  // namespace rc
