#include "input/keymap_cardputer_adv.h"

namespace rc {
namespace {

// Special keys, as codes outside printable ASCII.
constexpr char kBackspace = 1, kTab = 2, kFn = 3, kShift = 4, kEnter = 5, kCtrl = 6, kOpt = 7,
               kAlt = 8;

// Normal, shifted, by row and column. From M5Cardputer 1.1.1,
// src/utility/Keyboard/Keyboard.h (_key_value_map), github.com/m5stack/M5Cardputer.
constexpr char kMap[4][14][2] = {
    {{'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'},
     {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'},
     {kBackspace, kBackspace}},
    {{kTab, kTab}, {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'},
     {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'}, {'\\', '|'}},
    {{kFn, kFn}, {kShift, kShift}, {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'},
     {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '"'}, {kEnter, kEnter}},
    {{kCtrl, kCtrl}, {kOpt, kOpt}, {kAlt, kAlt}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'},
     {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {' ', ' '}},
};

// The key's normal-layer code, 0 if the number isn't wired. The TCA8418 runs a
// 7x8 matrix (key number - 1 = 10 * row + column); M5Cardputer 1.1.1's
// src/utility/Keyboard/KeyboardReader/TCA8418.cpp (remap) turns that into the
// 4x14 layout: column 2 * row (+1 for TCA columns 4-7), row TCA column % 4.
const char* lookup(uint8_t keyNum) {
  if (keyNum < 1) return nullptr;
  const uint8_t tcaRow = (keyNum - 1) / 10, tcaCol = (keyNum - 1) % 10;
  if (tcaRow > 6 || tcaCol > 7) return nullptr;
  return kMap[tcaCol % 4][tcaRow * 2 + (tcaCol > 3 ? 1 : 0)];
}

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

}  // namespace

KeyEvent CardputerKeys::key(uint8_t keyNum, bool pressed, uint32_t ms) {
  const char* k = lookup(keyNum);
  if (!k) return {};
  const char c = k[0];
  if (c == kShift || c == kFn) {
    (c == kShift ? shift_ : fn_) = pressed;
    return {};
  }
  if (!pressed) {
    if (c == kBackspace) bsHeld_ = false;
    return {};
  }
  if (c == kBackspace) {
    if (fn_) return action(Action::OpenHome);
    bsHeld_ = true;
    bsNextMs_ = ms + kCardputerRepeatDelayMs;
    return action(Action::Backspace);
  }
  if (c == kEnter) return action(Action::Select);
  const bool typing = textActive_ && !leftText_;
  if (c == '`') {
    if (!typing) return action(Action::Back);
    leftText_ = true;
    return action(Action::Up);
  }
  if (fn_ || !typing) {
    if (c == ';') return action(Action::Up);
    if (c == '.') return action(Action::Down);
    if (c == ',') return action(Action::Left);
    if (c == '/') return action(Action::Right);
  }
  if (c < ' ') return {};  // Tab, Ctrl, Opt, Alt
  leftText_ = false;
  KeyEvent e = action(Action::Char);
  e.ch = static_cast<char32_t>(k[shift_ ? 1 : 0]);
  return e;
}

void CardputerKeys::setTextActive(bool active) {
  if (!active) leftText_ = false;
  textActive_ = active;
}

KeyEvent CardputerKeys::poll(uint32_t ms) {
  if (!bsHeld_ || (int32_t)(ms - bsNextMs_) < 0) return {};
  bsNextMs_ = ms + kCardputerRepeatMs;
  return action(Action::Backspace);
}

}  // namespace rc
