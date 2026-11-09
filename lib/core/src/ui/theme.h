#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// RGB565 colours for one look of the UI.
struct Theme {
  uint16_t bg, fg, dim, barBg, barFg, hintBg, hintFg, time, sys, mentionBg, inputBg, cursorBg, cursorFg,
      selectBg, selectFg, drawerBg, drawerEdge, error;
  // Default and the Bubblegum and Top8 themes keep the website's per-user
  // nick hues; the others draw every nick in `nick`. Your own is always `selfNick`.
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

// Saved by number: append, never reorder. Dark … Top8 are cyberspace.online's;
// BubblegumDark and Top8Dark are our dark takes on its two light ones;
// SepiaDark and C64Dark are ours too.
enum class ThemeId : uint8_t {
  Default = 0, BlackWhite = 1, WhiteBlack = 2,
  Dark, Light, Lcd, C64, Vt320, Matrix, Poetry, Brutalist, Grid, Crypt, Bubblegum, Top8,
  BubblegumDark, Top8Dark, SepiaDark, C64Dark
};
constexpr int kThemeCount = 19;

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

// a moved pctB percent of the way to b, per RGB565 channel.
uint16_t blend565(uint16_t a, uint16_t b, int pctB);
// A chat bubble's fill (ChatStyle::Bubbles): yours tinted with selfNick, others' with fg.
uint16_t bubbleFill(const Theme& t, bool self);

ThemeId nextTheme(ThemeId id);  // wraps round
ThemeId themeFromIndex(int i);  // out of range gives Default
// Stable keys for settings, backup and web config: "default", "bw", "wb",
// then the site's names in lower case ("dark", "vt320", "top8"...).
const char* themeKey(ThemeId id);
bool parseThemeId(std::string_view key, ThemeId& out);  // leaves out unchanged on failure
// Shown in Settings and web config.
const char* themeLabel(ThemeId id);

}  // namespace rc
