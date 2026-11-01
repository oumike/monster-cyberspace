#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// Held Backspace repeats after kCardputerRepeatDelayMs, every kCardputerRepeatMs
// (the Pager's timing).
constexpr uint32_t kCardputerRepeatDelayMs = 500, kCardputerRepeatMs = 70;

// The M5Stack Cardputer ADV's TCA8418 keyboard, by key number (1-based, as the
// controller's event FIFO reports them). Shift and Fn are held modifiers: they
// apply while pressed. Fn+; . , / are Up, Down, Left, Right (as printed on the
// keys); ` (Esc) is Back; Enter is Select; Fn+Backspace is Home. Ctrl, Opt and
// Alt send nothing. Layout from M5Cardputer 1.1.1.
// Outside a text field ; . , / are the arrows without Fn. In one, Esc leaves
// it for the screen (sending Up: the newest message) and the plain arrows work
// until another printable key, which is typed and returns to the field.
class CardputerKeys {
 public:
  KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms);
  // A held Backspace's repeats; None otherwise.
  KeyEvent poll(uint32_t ms);
  // Whether the screen takes text (board::textEntry); a new field starts typing.
  void setTextActive(bool active);

 private:
  bool shift_ = false;
  bool fn_ = false;
  bool textActive_ = true;
  bool leftText_ = false;  // Esc pressed in the field: keys navigate
  bool bsHeld_ = false;
  uint32_t bsNextMs_ = 0;
};

}  // namespace rc
