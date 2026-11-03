#include "input/keymap_tdisplay_p4.h"

#include "input/function_keys.h"

namespace rc {
namespace {

constexpr uint8_t kKeyCount = 68;
constexpr uint8_t kCaps = 31, kAlt = 41, kUp = 50, kSym1 = 51, kShift = 53, kTab = 54,
                  kSym2 = 58, kLeft = 59, kDown = 60, kLight = 61, kBackspace = 63,
                  kEnter1 = 64, kEmoji = 65, kEnter2 = 66, kRight = 68;
constexpr uint8_t kShiftBit = 1, kSymBit = 2, kAltBit = 4;

// Base, shift, sym, by key number - 1. 0 in shift or sym falls back to base;
// 0 in base is a key handled by number (F1-F10: input/function_keys.h).
constexpr char kMap[kKeyCount][3] = {
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0},       {0, 0, 0},        //  1-4  F1-F4
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0},       {0, 0, 0},        //  5-8  F5-F8
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0},       {0, 0, 0},        //  9-12 F9, F10, Esc
    {'1', '!', '!'}, {'2', '@', '@'}, {'3', '#', '#'}, {'4', '$', '$'},  // 13-16
    {'5', '%', '%'}, {'6', '^', '^'}, {'7', '&', '&'}, {'8', '*', '*'},  // 17-20
    {'q', 'Q', '\''}, {'w', 'W', '_'}, {'e', 'E', '-'}, {'r', 'R', '+'}, // 21-24
    {'t', 'T', '='}, {'y', 'Y', '\\'}, {'u', 'U', '|'}, {'i', 'I', ';'}, // 25-28
    {'o', 'O', ':'}, {'p', 'P', '"'},  {0, 0, 0},       {'a', 'A', '~'}, // 29-32
    {'s', 'S', '['}, {'d', 'D', ']'}, {'f', 'F', '{'}, {'g', 'G', '}'},  // 33-36
    {'h', 'H', ','}, {'j', 'J', '`'}, {'k', 'K', '/'}, {'l', 'L', '?'},  // 37-40
    {0, 0, 0},       {'z', 'Z', 0},   {'x', 'X', 0},   {'c', 'C', 0},    // 41-44
    {'v', 'V', 0},   {'b', 'B', '.'}, {'n', 'N', '<'}, {'m', 'M', '>'},  // 45-48
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0},       {0, 0, 0},        // 49-52
    {0, 0, 0},       {0, 0, 0},       {' ', 0, 0},     {' ', 0, 0},      // 53-56
    {' ', 0, 0},     {0, 0, 0},       {0, 0, 0},       {0, 0, 0},        // 57-60
    {0, 0, 0},       {'9', '(', '('}, {0, 0, 0},       {0, 0, 0},        // 61-64
    {0, 0, 0},       {0, 0, 0},       {'0', ')', ')'}, {0, 0, 0},        // 65-68
};

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

// The action of a key handled by number, or None.
Action special(uint8_t keyNum) {
  switch (keyNum) {
    case 11: case 12: return Action::Back;
    case kUp: return Action::Up;
    case kDown: return Action::Down;
    case kLeft: return Action::Left;
    case kRight: return Action::Right;
    case kTab: return Action::Palette;
    case kBackspace: return Action::Backspace;
    case kEnter1: case kEnter2: return Action::Select;
    case kEmoji: return Action::Emoji;
    default: return Action::None;
  }
}

bool repeats(Action a) {
  return a == Action::Backspace || a == Action::Up || a == Action::Down ||
         a == Action::Left || a == Action::Right;
}

}  // namespace

KeyEvent TdisplayP4Keys::key(uint8_t keyNum, bool pressed, uint32_t ms) {
  if (!pressed) {
    if (keyNum == heldKey_) heldKey_ = 0;
    return {};
  }
  if (mods_ && ms - modMs_ > kP4ModifierMs) mods_ = 0;
  switch (keyNum) {
    case kShift: mods_ ^= kShiftBit; modMs_ = ms; return {};
    case kSym1: case kSym2: mods_ ^= kSymBit; modMs_ = ms; return {};
    case kAlt: mods_ ^= kAltBit; modMs_ = ms; return {};
    case kCaps: caps_ = !caps_; return {};
    case kLight: lightStep_ = true; return {};
    default: break;
  }
  if (keyNum < 1 || keyNum > kKeyCount) return {};
  const uint8_t mods = mods_;
  mods_ = 0;
  if ((mods & kAltBit) && keyNum == kBackspace) return action(Action::Back);
  if (keyNum <= 10) return action(functionKeyAction(keyNum));
  const Action a = special(keyNum);
  if (a != Action::None) {
    if (repeats(a)) {
      heldKey_ = keyNum;
      heldAction_ = a;
      nextRepeatMs_ = ms + kP4RepeatDelayMs;
    }
    return action(a);
  }
  const char* d = kMap[keyNum - 1];
  if (!d[0]) return {};
  char c = (mods & kSymBit) ? d[2] : (mods & kShiftBit) ? d[1] : d[0];
  if (!c) c = d[0];
  if (caps_ && d[0] >= 'a' && d[0] <= 'z') c = (mods & kShiftBit) ? d[0] : char(d[0] - 'a' + 'A');
  KeyEvent e = action(Action::Char);
  e.ch = static_cast<char32_t>(c);
  return e;
}

KeyEvent TdisplayP4Keys::poll(uint32_t ms) {
  if (!heldKey_ || (int32_t)(ms - nextRepeatMs_) < 0) return {};
  nextRepeatMs_ = ms + kP4RepeatMs;
  return action(heldAction_);
}

bool TdisplayP4Keys::takeLightStep() {
  const bool step = lightStep_;
  lightStep_ = false;
  return step;
}

}  // namespace rc
