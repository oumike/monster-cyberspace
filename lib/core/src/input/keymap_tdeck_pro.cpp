#include "input/keymap_tdeck_pro.h"

namespace rc {
namespace {

constexpr uint8_t kBackspace = 11, kEnter = 21, kAlt = 30, kShiftL = 31, kSym = 32, kSpace = 33,
                  kShiftR = 35, kKeyCount = 35;
constexpr uint8_t kShiftBit = 1, kSymBit = 2, kAltBit = 4;

// Plain, Sym, by key number - 1. Shift upper-cases the letters. Backspace,
// Enter and the modifiers are handled before this table.
constexpr char kMap[kKeyCount][2] = {
    {'p', '@'}, {'o', '+'}, {'i', '-'}, {'u', '_'}, {'y', ')'}, {'t', '('}, {'r', '3'},
    {'e', '2'}, {'w', '1'}, {'q', '#'}, {0, 0},     {'l', '"'}, {'k', '\''}, {'j', ';'},
    {'h', ':'}, {'g', '/'}, {'f', '6'}, {'d', '5'}, {'s', '4'}, {'a', '*'}, {0, 0},
    {'$', '$'}, {'m', '.'}, {'n', ','}, {'b', '!'}, {'v', '?'}, {'c', '9'}, {'x', '8'},
    {'z', '7'}, {0, 0},     {0, 0},     {0, 0},     {' ', ' '}, {0, '0'},   {0, 0},
};

Action altChord(uint8_t keyNum) {
  switch (keyNum) {
    case 9: return Action::Up;              // W
    case 20: return Action::Left;           // A
    case 19: return Action::Down;           // S
    case 18: return Action::Right;          // D
    case kBackspace: return Action::Back;
    case kEnter: return Action::Palette;
    case 15: return Action::OpenHome;       // H
    case 27: return Action::OpenCirc;       // C
    case 23: return Action::OpenCmail;      // M
    case 17: return Action::OpenFeed;       // F
    default: return Action::None;
  }
}

bool repeats(Action a) {
  return a == Action::Backspace || a == Action::Up || a == Action::Down || a == Action::Left ||
         a == Action::Right;
}

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

}  // namespace

KeyEvent TdeckProKeys::key(uint8_t keyNum, bool pressed, uint32_t ms) {
  if (!pressed) {
    if (keyNum == kAlt) altDown_ = false;
    if (keyNum == heldKey_ || (keyNum == kAlt && heldAction_ != Action::Backspace)) heldKey_ = 0;
    return {};
  }
  if (mods_ && ms - modMs_ > kTdeckProModifierMs) mods_ = 0;
  if (keyNum == kAlt || keyNum == kShiftL || keyNum == kShiftR || keyNum == kSym) {
    if (keyNum == kAlt) altDown_ = true;
    mods_ ^= keyNum == kAlt ? kAltBit : keyNum == kSym ? kSymBit : kShiftBit;
    modMs_ = ms;
    if (keyNum != kSym) return {};
    KeyEvent e = action(Action::Symbols);
    e.modifier = true;
    return e;
  }
  if (keyNum < 1 || keyNum > kKeyCount) return {};
  const uint8_t mods = mods_;
  mods_ = 0;
  KeyEvent e;
  if (altDown_ || (mods & kAltBit)) {
    e = action(altChord(keyNum));
  } else if (keyNum == kEnter) {
    e = action(mods & kSymBit ? Action::Palette : Action::Select);
  } else if (keyNum == kBackspace) {
    e = action(mods & kSymBit ? Action::Back : Action::Backspace);
  } else if (keyNum == kSpace && (mods & kSymBit)) {
    e = action(Action::OpenHome);
  } else {
    char c = kMap[keyNum - 1][mods & kSymBit ? 1 : 0];
    if (!c) return {};
    if ((mods & kShiftBit) && !(mods & kSymBit) && c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    e = action(Action::Char);
    e.ch = static_cast<char32_t>(c);
  }
  if (repeats(e.action)) {
    heldKey_ = keyNum;
    heldAction_ = e.action;
    heldNextMs_ = ms + kTdeckProRepeatDelayMs;
  }
  return e;
}

KeyEvent TdeckProKeys::poll(uint32_t ms) {
  if (!heldKey_ || (int32_t)(ms - heldNextMs_) < 0) return {};
  heldNextMs_ = ms + kTdeckProRepeatMs;
  return action(heldAction_);
}

}  // namespace rc
