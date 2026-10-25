// Temporary hardware bring-up for Task 14: a glyph test pattern, live key
// logging, the text-size toggle, screen off/wake and a screensaver
// placeholder. Replaced in Task 19.

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <string>

#include "core_version.h"
#include "hal/board.h"
#include "input/keymap_m9.h"
#include "ui/grid.h"
#include "ui/nick_color.h"
#include "ui/renderer.h"
#include "ui/theme.h"
#include "util/utf8.h"

using namespace rc;

namespace {

Renderer renderer;
TextSize textSize = TextSize::Compact;
Grid grid(1, 1);
enum class Screen { On, Dark, Saver };
Screen screen = Screen::On;
uint32_t saverUntilMs = 0;
constexpr uint32_t kSaverMs = 10000;
std::string lastKey = "press a key";

const char* actionName(Action a) {
  switch (a) {
    case Action::None: return "None";
    case Action::Char: return "Char";
    case Action::Backspace: return "Backspace";
    case Action::Select: return "Select";
    case Action::Back: return "Back";
    case Action::Up: return "Up";
    case Action::Down: return "Down";
    case Action::Left: return "Left";
    case Action::Right: return "Right";
    case Action::OpenCirc: return "OpenCirc";
    case Action::OpenHome: return "OpenHome";
    case Action::OpenCmail: return "OpenCmail";
    case Action::OpenFeed: return "OpenFeed";
    case Action::OpenMap: return "OpenMap";
    case Action::Palette: return "Palette";
    case Action::ClearLine: return "ClearLine";
    case Action::ScreenOff: return "ScreenOff";
  }
  return "?";
}

void drawPattern() {
  const Theme& t = defaultTheme();
  int cols, rows;
  gridDims(board::panelWidth(), board::panelHeight(), textSize, cols, rows);
  grid.resize(cols, rows);
  grid.fill(t.bg);

  std::u32string head =
      utf8ToU32(std::string(" rhino-cyberspace ") + RC_VERSION + " " + board::name() +
                (textSize == TextSize::Compact ? " compact" : " comfortable"));
  grid.fillRow(0, t.barBg);
  grid.putText(0, 0, head, t.barFg, t.barBg, cols);

  int row = 1, col = 0;
  for (char32_t c = 0x20; c <= 0x7E; ++c) {
    grid.put(col, row, c, t.fg, t.bg);
    if (++col >= cols) col = 0, ++row;
  }
  ++row;
  grid.putText(0, row++,
               U"\u250C\u2500\u252C\u2500\u2510 \u25CF\u25CC\u2715 \u2582\u2584\u2586\u2588 "
               U"\u2580\u2592 \u00B7\u2026 \u2190\u2191\u2192\u2193 \u2022\u25B6\u2713",
               t.fg, t.bg, cols);
  grid.putText(0, row++,
               U"\u251C\u2500\u253C\u2500\u2524 \U0001F3B2\U0001F3B1\U0001F960 \uFFFD \u4E2D "
               U"\u00E9\u00FC\u00DF\u00A9",
               t.fg, t.bg, cols);
  grid.putText(0, row++, U"\u2514\u2500\u2534\u2500\u2518", t.fg, t.bg, cols);
  ++row;
  int c = 0;
  for (const char* nick : {"tardibyte", "rain", "oumike", "genghis", "zed"}) {
    bool self = std::string(nick) == "oumike";
    c += grid.putText(c, row, utf8ToU32(nick), nickColor(nick, self), t.bg, cols - c) + 1;
  }
  ++row;
  grid.putText(0, row, U"14:02", t.time, t.bg, cols);
  grid.putText(6, row, U"* system line", t.sys, t.bg, cols - 6);
}

void drawLastKey() {
  const Theme& t = defaultTheme();
  int row = grid.rows() - 1;
  grid.fillRow(row, t.hintBg);
  grid.putText(0, row, utf8ToU32(lastKey), t.hintFg, t.hintBg, grid.cols());
}

void redrawAll() {
  drawPattern();
  drawLastKey();
  renderer.draw(grid, textSize);
}

// Placeholder for the screensaver: the real one (Task 20) shows the last few
// stored messages of the current room.
void drawSaver() {
  const Theme& t = defaultTheme();
  grid.fill(t.bg);
  grid.putText(1, 1, U"screensaver", t.dim, t.bg, grid.cols() - 1);
  grid.putText(1, 3, U"the last few messages", t.fg, t.bg, grid.cols() - 1);
  grid.putText(1, 4, U"will show here", t.fg, t.bg, grid.cols() - 1);
  grid.putText(1, grid.rows() - 2, U"hold \u25CF to wake", t.dim, t.bg, grid.cols() - 1);
  renderer.draw(grid, textSize);
}

void goDark() {
  screen = Screen::Dark;
  board::displaySleep(true);
}

}  // namespace

void setup() {
  // Chat data is many small strings: send everything over 64 bytes to PSRAM.
  heap_caps_malloc_extmem_enable(64);
  Serial.begin(115200);
  Serial.printf("[boot] rhino-cyberspace %s board=M9\n", RC_VERSION);
  board::begin();
  if (!renderer.begin()) Serial.println("[ui] no memory for the frame buffer");
  redrawAll();
  board::backlight(true);
  Serial.printf("[boot] battery %d%% boot=%d\n", board::batteryPercent(),
                board::bootButtonHeld());
}

void loop() {
  if (screen == Screen::Saver && (int32_t)(millis() - saverUntilMs) >= 0) goDark();
  uint8_t raw;
  if (board::pollKey(raw)) {
    KeyEvent e = mapM9Key(raw, board::earlyKeyboard());
    Serial.printf("[key] raw=0x%02X action=%s\n", raw, actionName(e.action));
    if (screen != Screen::On) {
      // Only a centre hold wakes and only a centre tap shows the screensaver,
      // so a stray press can't act on a dark screen.
      if (wakesScreen(e)) {
        bool wasDark = screen == Screen::Dark;
        screen = Screen::On;
        redrawAll();
        if (wasDark) board::displaySleep(false);
      } else if (peeksScreen(e)) {
        saverUntilMs = millis() + kSaverMs;
        if (screen == Screen::Dark) {
          screen = Screen::Saver;
          drawSaver();
          board::displaySleep(false);
        }
      }
      return;
    }
    char buf[48];
    snprintf(buf, sizeof buf, "raw=0x%02X action=%s", raw, actionName(e.action));
    lastKey = buf;
    if (e.action == Action::Char) lastKey += std::string(" '") + static_cast<char>(e.ch) + "'";
    if (e.action == Action::Palette) {
      textSize = textSize == TextSize::Compact ? TextSize::Comfortable : TextSize::Compact;
      drawPattern();
    } else if (e.action == Action::ScreenOff) {
      goDark();
      return;
    }
    drawLastKey();
    renderer.draw(grid, textSize);
  }
  delay(5);
}
