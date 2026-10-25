#include "input/keymap_m9.h"

namespace rc {

KeyEvent mapM9Key(uint8_t raw, bool earlyController) {
  KeyEvent e;
  switch (raw) {
    case 0x08: case 0x7F: e.action = Action::Backspace; break;
    case 0x0D: case 0x0A: e.action = Action::Select; break;
    case 0x81: e.action = Action::OpenCirc; break;
    case 0x82: e.action = Action::OpenHome; break;
    case 0x83: e.action = Action::OpenCmail; break;
    case 0x84: e.action = Action::OpenFeed; break;
    case 0x85: e.action = Action::OpenMap; break;
    case 0x86: if (earlyController) e.action = Action::Back; break;
    case 0x87: if (!earlyController) e.action = Action::Back; break;
    case 0x88: case 0x90: e.action = Action::Palette; break;
    case 0x89: e.action = Action::ClearLine; break;
    case 0xA3: e.action = Action::ScreenOff; break;
    case 0xB4: e.action = Action::Left; break;
    case 0xB5: e.action = Action::Up; break;
    case 0xB6: e.action = Action::Down; break;
    case 0xB7: e.action = Action::Right; break;
    default:
      if (raw >= 0x20 && raw <= 0x7E) {
        e.action = Action::Char;
        e.ch = raw;
      }
      break;
  }
  return e;
}

bool wakesScreen(const KeyEvent& e) { return e.action == Action::Select; }

}  // namespace rc
