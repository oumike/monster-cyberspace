// SPDX-License-Identifier: GPL-3.0-or-later
#include "input/keymap_tab5.h"

namespace rc {
namespace {

// From the keyboard firmware's key_value_map; 0 for the keys below.
constexpr char kBase[Tab5Keys::kRows][Tab5Keys::kCols + 1] = {
    "\0001234567890-+\0",
    "`!@#$%^&*()[]\\",
    "\0qwertyuiop;'\0",
    "\0\0asdfghjkl\0_\0",
    "\0\0zxcvbnm.\0\0\0 ",
};
// The Sym layer (and Shift's, for keys that are not letters); 0: as kBase.
constexpr char kSym[Tab5Keys::kRows][Tab5Keys::kCols + 1] = {
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "~?\0\0\0\0\0\0/<>{}|",
    "\0\0\0\0\0\0\0\0\0\0\0:\"\0",
    "\0\0\0\0\0\0\0\0\0\0\0\0=\0",
    "\0\0\0\0\0\0\0\0\0,\0\0\0\0",
};

enum class Special { None, Esc, Del, Tab, Backspace, Sym, Aa, Enter, Ctrl, Alt, Up, Down, Left, Right };

Special special(int row, int col) {
  switch (row << 4 | col) {
    case 0x00: return Special::Esc;
    case 0x0D: return Special::Del;
    case 0x20: return Special::Tab;
    case 0x2D: return Special::Backspace;
    case 0x30: return Special::Sym;
    case 0x31: return Special::Aa;
    case 0x3B: return Special::Up;
    case 0x3D: return Special::Enter;
    case 0x40: return Special::Ctrl;
    case 0x41: return Special::Alt;
    case 0x4A: return Special::Left;
    case 0x4B: return Special::Down;
    case 0x4C: return Special::Right;
    default: return Special::None;
  }
}

KeyEvent act(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

}  // namespace

KeyEvent Tab5Keys::key(uint8_t event, uint32_t ms) {
  const int row = (event >> 4) & 0x07, col = event & 0x0F;
  if (row >= kRows || col >= kCols) return {};
  return (event & 0x80) ? press(row, col, ms) : release(row, col, ms);
}

KeyEvent Tab5Keys::press(int row, int col, uint32_t ms) {
  const Special s = special(row, col);
  switch (s) {
    case Special::Aa:
      aaDown_ = true;
      aaUsed_ = false;
      aaDownMs_ = ms;
      return {};
    case Special::Sym:
      symDown_ = true;
      symUsed_ = false;
      return {};
    case Special::Ctrl:
      ctrlDown_ = true;
      ctrlUsed_ = false;
      return {};
    case Special::Alt:
      altDown_ = true;
      return {};
    default:
      break;
  }
  aaUsed_ = aaUsed_ || aaDown_;
  symUsed_ = symUsed_ || symDown_;
  ctrlUsed_ = ctrlUsed_ || ctrlDown_;

  Action a = Action::None;
  switch (s) {
    case Special::Esc: a = Action::Back; break;
    case Special::Enter: a = Action::Select; break;
    case Special::Tab: a = Action::Palette; break;
    case Special::Del:
    case Special::Backspace: a = Action::Backspace; break;
    case Special::Up: a = Action::Up; break;
    case Special::Down: a = Action::Down; break;
    case Special::Left: a = Action::Left; break;
    case Special::Right: a = Action::Right; break;
    default: break;
  }
  if (a != Action::None) {
    heldKey_ = row * kCols + col;
    heldAction_ = a;
    nextRepeatMs_ = ms + kTab5RepeatDelayMs;
    return act(a);
  }

  const char base = kBase[row][col];
  if (!base) return {};
  if (altDown_) {
    if (base >= '0' && base <= '9') return act(functionKeyAction(base == '0' ? 10 : base - '0'));
    return {};
  }
  if (ctrlDown_) return {};

  const bool shift = aaDown_ || shiftPending_;
  char c = base;
  if (base >= 'a' && base <= 'z') {
    if (shift || caps_) c = char(base - 'a' + 'A');
  } else if ((shift || symDown_ || symPending_) && kSym[row][col]) {
    c = kSym[row][col];
  }
  shiftPending_ = false;
  symPending_ = false;
  KeyEvent e = act(Action::Char);
  e.ch = char32_t(static_cast<unsigned char>(c));
  return e;
}

KeyEvent Tab5Keys::release(int row, int col, uint32_t ms) {
  if (row * kCols + col == heldKey_) heldKey_ = -1;
  switch (special(row, col)) {
    case Special::Aa:
      aaDown_ = false;
      if (aaUsed_ || ms - aaDownMs_ > kTab5AaHoldMs) return {};
      if (caps_) {
        caps_ = false;
        shiftPending_ = false;
      } else if (shiftPending_ && ms - aaTapMs_ <= kTab5AaDoubleMs) {
        caps_ = true;
        shiftPending_ = false;
      } else {
        shiftPending_ = true;
      }
      aaTapMs_ = ms;
      return {};
    case Special::Sym:
      symDown_ = false;
      if (!symUsed_) symPending_ = !symPending_;
      return {};
    case Special::Ctrl:
      ctrlDown_ = false;
      return ctrlUsed_ ? KeyEvent{} : act(Action::Emoji);
    case Special::Alt:
      altDown_ = false;
      return {};
    default:
      return {};
  }
}

KeyEvent Tab5Keys::poll(uint32_t ms) {
  if (heldKey_ < 0 || int32_t(ms - nextRepeatMs_) < 0) return {};
  nextRepeatMs_ = ms + kTab5RepeatMs;
  return act(heldAction_);
}

}  // namespace rc
