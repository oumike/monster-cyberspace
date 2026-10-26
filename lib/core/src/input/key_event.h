#pragma once

namespace rc {

// A board-independent key press. Each board's HAL maps its own input to one
// (board::pollKey).
enum class Action {
  None, Char, Backspace, Select, Back, Up, Down, Left, Right,
  OpenCirc, OpenHome, OpenCmail, OpenFeed, OpenMap, Palette, ClearLine, ScreenOff
};

struct KeyEvent {
  Action action = Action::None;
  char32_t ch = 0;  // set for Action::Char
};

// Only a centre hold (ScreenOff) wakes a dark screen, so a stray press or tap
// in a pocket can't light it or type into it.
bool wakesScreen(const KeyEvent& e);

// A centre tap on a dark screen shows the screensaver (the last few messages)
// without waking.
bool peeksScreen(const KeyEvent& e);

}  // namespace rc
