#pragma once

#include <cstddef>
#include <cstdint>

#include "input/key_event.h"

// Forward-declared so this header stays free of FS.h.
namespace fs {
class FS;
}

// The hardware abstraction each board implements. One implementation is
// compiled per firmware env (src/hal/<board>/).
namespace board {

void begin();
const char* name();  // "M9", "WioL2"; also used in the setup AP name
// The release asset slug (scripts/release.sh env_out_name): "m9", "wio-l2", ...
const char* otaSlug();
int panelWidth();
int panelHeight();
// Sends rows [y0, y1) of a full panelWidth() x panelHeight() RGB565 frame.
void pushRows(const uint16_t* frame, int y0, int y1);
void backlight(bool on);
// Dims the backlight: 255 is the board's normal on level, 0 off. Boards with
// an on/off backlight treat any level above 0 as on.
void backlightLevel(uint8_t level);
void displaySleep(bool sleep);
// The keyboard backlight. hasKbLight: the board has one to set;
// kbLightDefault: its rc::KbLight index before any is saved. kbLight sets it,
// duty 0..255, lit only while screenOn (the M9's controller lights the keys
// itself on a key press, so there it is that light's brightness, and 0
// switches it off until a power cycle).
bool hasKbLight();
uint8_t kbLightDefault();
void kbLight(uint8_t duty, bool screenOn);
// The last line of the screensaver, telling the user how to wake the screen.
const char* wakeHint();
// The next pending key press, already mapped to a portable key.
bool pollKey(rc::KeyEvent& key);
int batteryPercent();  // -1 when unknown
bool externalPower();
// Returns at once; the tone plays in the background.
void beep(int freqHz, int ms);
// Raw touch, for boards with a touch panel. readTouch is true while a finger
// is down, with x, y in the panelWidth() x panelHeight() frame.
bool hasTouch();
bool readTouch(int16_t& x, int16_t& y);
// Boards that can turn the panel: canRotate is a compile-time property.
// setRotation turns the panel scan and the touch transform together, and
// panelWidth/panelHeight follow (rotation r as in ui/rotation.h). A no-op on
// boards that can't rotate.
bool canRotate();
// A physical keyboard (the on-screen one is shown only without it).
bool hasKeyboard();
// Any keys or buttons that move a selection (keyboard, d-pad, trackball,
// buttons). Without them no cursor is drawn: touch picks by position.
bool hasKeys();
// Backspace means Back when nothing on screen consumes it.
bool backspaceIsBack();
// Any key, roll or touch wakes a dark screen, not only the board's wake key.
bool anyInputWakes();
// With anyInputWakes on a touch board: only a double tap wakes the screen,
// so a brush against the panel doesn't.
bool doubleTapWakes();
// True when the only arrows are Up/Down (a wheel): pickers then step one cell
// at a time instead of moving in two dimensions.
bool upDownOnly();
// Lists get an Up / Select / Down button strip at their right edge
// (ui/widgets/side_nav.h), for touch panels where picking a row is a chore.
// The selection outline is drawn, as on keyed boards.
bool sideNav();
// The UI says each frame whether a text field takes typing, for keys only
// read then (the T-Deck Plus's mic key, polled from the raw key matrix).
void textEntry(bool active);
// An e-paper panel: slow refreshes, and the image stays with the power off.
// The UI then keeps still (no animation), uses rc::paperTheme only, and the
// panel's pushRows only marks what changed; the board refreshes it from pollKey.
bool eink();
void setRotation(uint8_t r);
// GPS receiver. On: power, a 10 ms reset pulse, then the UART up. Off: the
// UART down, then power off.
bool hasGps();
void gpsPower(bool on);
// Raw NMEA bytes. Never blocks; 0 while off.
size_t gpsRead(uint8_t* buf, size_t n);
// Battery-backed RTC, in UTC seconds. rtcRead is false with no RTC, an I2C
// error, or a time the RTC flags as unreliable (lost power).
bool rtcRead(int64_t& epochUtc);
bool rtcWrite(int64_t epochUtc);

// Removable SD card, mounted one operation at a time. hasSdSlot is a compile
// -time property of the board. sdMount mounts the card and returns its
// filesystem, or nullptr with no slot or no card (the card may have been
// pulled since the last mount). Pair every non-null sdMount with sdUnmount.
// Not thread-safe: only the backup store calls these.
bool hasSdSlot();
fs::FS* sdMount();
void sdUnmount();

}  // namespace board
