// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "input/function_keys.h"
#include "input/key_event.h"

namespace rc {

// Held Backspace, del and arrows repeat after kTab5RepeatDelayMs, every
// kTab5RepeatMs.
constexpr uint32_t kTab5RepeatDelayMs = 450, kTab5RepeatMs = 70;
// Aa tapped twice within this is Caps Lock; held longer than kTab5AaHoldMs
// it is a plain Shift, as the keyboard's own firmware does.
constexpr uint32_t kTab5AaDoubleMs = 500, kTab5AaHoldMs = 1000;

// The Tab5 Keyboard has no function keys: Alt + a digit reaches them. F10's
// help shows this, Ctrl tapped alone and Tab.
constexpr FunctionKey kTab5FunctionKeys[] = {
    {U"Alt+1", U"home", Action::OpenHome},         {U"Alt+2", U"messages", Action::OpenCirc},
    {U"Alt+3", U"c-mail", Action::OpenCmail},      {U"Alt+4", U"feed", Action::OpenFeed},
    {U"Alt+5", U"guilds", Action::OpenMap},        {U"Alt+6", U"profile", Action::OpenProfile},
    {U"Alt+7", U"settings", Action::OpenSettings}, {U"Alt+0", U"navigational help", Action::KeysHelp},
    {U"Ctrl", U"emoji, palette", Action::Emoji},   {U"Tab", U"menus, post", Action::Palette},
};
constexpr size_t kTab5FunctionKeyCount = std::size(kTab5FunctionKeys);

// The M5Stack Tab5 Keyboard in its Normal mode: its STM32 queues one byte
// per key change (bit 7 pressed, bits 6-4 row, bits 3-0 column) and the
// layers live here. Aa and Sym behave as the keyboard's own firmware: Aa
// tapped is a one-shot Shift, tapped twice Caps Lock, held Shift; Sym held
// is the second layer, tapped alone the next key's. Keys as spec
// 2026-11-17-cyberspace-tab5-design.md §4.
class Tab5Keys {
 public:
  static constexpr int kRows = 5, kCols = 14;

  // One event byte from the keyboard; the key it makes, or None.
  KeyEvent key(uint8_t event, uint32_t ms);
  // A held Backspace, del or arrow's repeat, or None.
  KeyEvent poll(uint32_t ms);

  bool capsLock() const { return caps_; }
  bool shiftPending() const { return shiftPending_; }

 private:
  KeyEvent press(int row, int col, uint32_t ms);
  KeyEvent release(int row, int col, uint32_t ms);

  bool aaDown_ = false, aaUsed_ = false, shiftPending_ = false, caps_ = false;
  uint32_t aaDownMs_ = 0, aaTapMs_ = 0;
  bool symDown_ = false, symUsed_ = false, symPending_ = false;
  bool ctrlDown_ = false, ctrlUsed_ = false, altDown_ = false;
  int heldKey_ = -1;
  Action heldAction_ = Action::None;
  uint32_t nextRepeatMs_ = 0;
};

}  // namespace rc
