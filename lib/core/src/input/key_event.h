#pragma once

#include <cstdint>

namespace rc {

// A board-independent key press. Each board's HAL maps its own input to one
// (board::pollKey).
enum class Action {
  None, Char, Backspace, Select, Back, Up, Down, Left, Right,
  OpenCirc, OpenHome, OpenCmail, OpenFeed, OpenMap, Palette, ClearLine, ScreenOff,
  Activate, Focus, PowerTap,
  Options,  // an edge swipe from the right: the screen's options
  Symbols,  // the symbol table while typing; the palette elsewhere
  Emoji     // the emoji tray while typing; the palette elsewhere
};

// A list a touch Row target belongs to.
enum class ListId : uint8_t { Main, Menu, Drawer, Log };

struct KeyEvent {
  Action action = Action::None;
  char32_t ch = 0;              // Char
  int16_t index = -1;           // Activate: the row; Focus: the text position
  uint8_t target = 0;           // Activate: a ListId; Focus: the screen's field id
  Action then = Action::None;   // Activate: sent once the selection has moved
  bool modifier = false;        // Symbols from a key that is also a modifier
                                // (the Pager's Sym, the T-Decks' mic):
                                // ignored outside typing
};

// With anyInputWakes false, only a centre hold (ScreenOff) wakes a dark screen,
// so a stray press or tap in a pocket can't light it or type into it. Boards
// that set it (board::anyInputWakes) wake on any action except None.
bool wakesScreen(const KeyEvent& e, bool anyInputWakes);

// A centre tap on a dark screen shows the screensaver (the last few messages)
// without waking.
bool peeksScreen(const KeyEvent& e);

}  // namespace rc
