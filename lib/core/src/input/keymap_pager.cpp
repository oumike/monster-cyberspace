#include "input/keymap_pager.h"

namespace rc {
namespace {

constexpr uint8_t kEnter = 20, kSym = 21, kShift = 29, kBackspace = 30, kSpace = 31;
constexpr uint8_t kShiftBit = 1, kSymBit = 2;

// Normal, Sym, by key number - 1. Shift upper-cases the letters.
constexpr char kMap[31][2] = {
    {'q', '1'}, {'w', '2'}, {'e', '3'}, {'r', '4'}, {'t', '5'}, {'y', '6'}, {'u', '7'},
    {'i', '8'}, {'o', '9'}, {'p', '0'}, {'a', '*'}, {'s', '/'}, {'d', '+'}, {'f', '-'},
    {'g', '='}, {'h', ':'}, {'j', '\''}, {'k', '"'}, {'l', '@'}, {0, 0},  {0, 0},
    {'z', '_'}, {'x', '$'}, {'c', ';'}, {'v', '?'}, {'b', '!'}, {'n', ','}, {'m', '.'},
    {0, 0},     {0, 0},     {' ', ' '},
};

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

}  // namespace

KeyEvent PagerKeys::key(uint8_t keyNum, bool pressed, uint32_t ms) {
  if (!pressed) {
    if (keyNum == kBackspace) bsHeld_ = false;
    return {};
  }
  if (mods_ && ms - modMs_ > kPagerModifierMs) mods_ = 0;
  if (keyNum == kShift || keyNum == kSym) {
    mods_ ^= keyNum == kShift ? kShiftBit : kSymBit;
    modMs_ = ms;
    return {};
  }
  if (keyNum < 1 || keyNum > kSpace) return {};
  const uint8_t mods = mods_;
  mods_ = 0;
  if (keyNum == kEnter) return action(mods & kSymBit ? Action::Palette : Action::Select);
  if (keyNum == kBackspace) {
    if (mods & kSymBit) return action(Action::OpenHome);
    bsHeld_ = true;
    bsNextMs_ = ms + kPagerRepeatDelayMs;
    return action(Action::Backspace);
  }
  char c = kMap[keyNum - 1][mods & kSymBit ? 1 : 0];
  if (!c) return {};
  if ((mods & kShiftBit) && !(mods & kSymBit) && c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  KeyEvent e = action(Action::Char);
  e.ch = static_cast<char32_t>(c);
  return e;
}

KeyEvent PagerKeys::poll(uint32_t ms) {
  if (!bsHeld_ || (int32_t)(ms - bsNextMs_) < 0) return {};
  bsNextMs_ = ms + kPagerRepeatMs;
  return action(Action::Backspace);
}

}  // namespace rc
