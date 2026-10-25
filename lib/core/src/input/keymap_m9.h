#pragma once

#include <cstdint>

namespace rc {

enum class Action {
  None, Char, Backspace, Select, Back, Up, Down, Left, Right,
  OpenCirc, OpenHome, OpenCmail, OpenFeed, OpenMap, Palette, ClearLine, ScreenOff
};

struct KeyEvent {
  Action action = Action::None;
  char32_t ch = 0;  // set for Action::Char
};

// Maps a raw STC8H key code. Early controllers (hw 0x03, fw 0x10) report
// Back as 0x86; production ones use 0x87.
KeyEvent mapM9Key(uint8_t raw, bool earlyController);

// Only a centre hold (ScreenOff) wakes a dark screen, so a stray press or tap
// in a pocket can't light it or type into it.
bool wakesScreen(const KeyEvent& e);

// A centre tap on a dark screen shows the screensaver (the last few messages)
// without waking.
bool peeksScreen(const KeyEvent& e);

}  // namespace rc
