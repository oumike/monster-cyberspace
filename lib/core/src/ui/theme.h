#pragma once

#include <cstdint>

namespace rc {

// RGB565 colours for one look of the UI.
struct Theme {
  uint16_t bg, fg, dim, barBg, barFg, hintBg, hintFg, time, sys, mentionBg, inputBg, cursorBg, cursorFg,
      selectBg, selectFg, drawerBg, drawerEdge, error;
};

// The "terminal" look of the mockups: green on black.
const Theme& defaultTheme();

}  // namespace rc
