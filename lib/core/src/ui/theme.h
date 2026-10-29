#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// RGB565 colours for one look of the UI.
struct Theme {
  uint16_t bg, fg, dim, barBg, barFg, hintBg, hintFg, time, sys, mentionBg, inputBg, cursorBg, cursorFg,
      selectBg, selectFg, drawerBg, drawerEdge, error;
  // Default, Bubblegum and Top8 keep the website's per-user nick hues; the
  // others draw every nick in `nick`. Your own is always `selfNick`.
  bool colorNicks;
  uint16_t nick, selfNick;
  // E-paper: a highlight that would be a solid block (a picked symbol, a
  // focused checkbox, an error banner) is drawn as an outline instead.
  bool outlined;
  // E-paper: nicks in the room log are drawn bold, as colour can't set them apart.
  bool boldNicks = false;
  // The per-user nick colours' HSL lightness, in percent: light on a dark
  // background, dark on a light one.
  uint8_t nickLightness = 68;
};

// Saved by number: append, never reorder. Dark onwards are cyberspace.online's.
enum class ThemeId : uint8_t {
  Default = 0, BlackWhite = 1, WhiteBlack = 2,
  Dark, Light, Lcd, C64, Vt320, Matrix, Poetry, Brutalist, Grid, Crypt, Bubblegum, Top8
};
constexpr int kThemeCount = 15;

const Theme& theme(ThemeId id);
// The "terminal" look of the mockups: green on black.
const Theme& defaultTheme();
// E-paper boards (board::eink) always use this: black on white, no greys, so
// the panel's 1-bit conversion keeps every line and letter, and outlined.
const Theme& paperTheme();

// Whether lists, logs and Home tiles outline their selection. Off on boards
// with no keys, where a tap picks by position and a cursor only misleads.
bool cursorShown();
void setCursorShown(bool on);

ThemeId nextTheme(ThemeId id);  // wraps round
ThemeId themeFromIndex(int i);  // out of range gives Default
// Stable keys for settings, backup and web config: "default", "bw", "wb",
// then the site's names in lower case ("dark", "vt320", "top8"...).
const char* themeKey(ThemeId id);
bool parseThemeId(std::string_view key, ThemeId& out);  // leaves out unchanged on failure
// Shown in Settings and web config.
const char* themeLabel(ThemeId id);

}  // namespace rc
