#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// RGB565 colours for one look of the UI.
struct Theme {
  uint16_t bg, fg, dim, barBg, barFg, hintBg, hintFg, time, sys, mentionBg, inputBg, cursorBg, cursorFg,
      selectBg, selectFg, drawerBg, drawerEdge, error;
  // Default keeps the website's per-user nick colours; the monochrome themes
  // draw every nick in `nick` and your own in `selfNick`.
  bool colorNicks;
  uint16_t nick, selfNick;
};

enum class ThemeId : uint8_t { Default = 0, BlackWhite = 1, WhiteBlack = 2 };
constexpr int kThemeCount = 3;

const Theme& theme(ThemeId id);
// The "terminal" look of the mockups: green on black.
const Theme& defaultTheme();

ThemeId nextTheme(ThemeId id);  // wraps round
ThemeId themeFromIndex(int i);  // out of range gives Default
// Stable keys for settings and web config: "default", "bw", "wb".
const char* themeKey(ThemeId id);
bool parseThemeId(std::string_view key, ThemeId& out);  // leaves out unchanged on failure
// Shown in Settings and web config.
const char* themeLabel(ThemeId id);

}  // namespace rc
