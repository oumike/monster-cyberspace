#include "input/keymap_tdeck.h"

namespace rc {

KeyEvent mapTdeckKey(uint8_t raw) {
  KeyEvent e;
  if (raw == 0x08) {
    e.action = Action::Backspace;
  } else if (raw == 0x0D || raw == 0x0A) {
    e.action = Action::Select;
  } else if (raw >= 0x20 && raw <= 0x7E) {
    e.action = Action::Char;
    e.ch = raw;
  }
  return e;
}

}  // namespace rc
