#pragma once

#include <cstddef>
#include <cstdint>

#include "input/function_keys.h"
#include "input/key_event.h"

// Forward-declared so this header stays free of FS.h.
namespace fs {
class FS;
}

namespace rc {
enum class TextSize;  // ui/grid.h
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
// A board that keeps no frame (the X4, without PSRAM) takes redraw and
// returns true: the UI then calls pushRows(nullptr, y0, y1) to say which rows
// changed, and redraw, called from pollKey, draws every line from the top
// through pushRows. Default: false (redraw unused).
bool setRedraw(void (*redraw)());
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
// An RGB notification light (the Mesh Deck's LEDs). notifyLight sets it:
// bit 0 red, bit 1 green, bit 2 blue, 0 off. A no-op without one.
bool hasNotifyLight();
void notifyLight(uint8_t rgb);
// Lights of their own for C-Mail and the Feed (the Tanmatsu's A and B, the
// Mesh Deck's keyboard halves), in notifyLight's rgb bits. With one, that
// source blinks there instead of on the notification light; Settings and web
// config name it cmailLightName / feedLightName (UTF-8). Default: none.
bool hasCmailLight();
void cmailLight(uint8_t rgb);
const char* cmailLightName();
bool hasFeedLight();
void feedLight(uint8_t rgb);
const char* feedLightName();
// A power light and a Wi-Fi light (the Tanmatsu's LEDs), each set in
// notifyLight's rgb bits. hasLedLevel: the status LEDs (these and the
// notification light) dim together, ledLevel giving each lit channel's value
// (rc::ledLevelValue), 0 off. Default: none of them.
bool hasPowerLight();
void powerLight(uint8_t rgb);
bool hasWifiLight();
void wifiLight(uint8_t rgb);
bool hasLedLevel();
void ledLevel(uint8_t value);
// The function-key table the help (Action::KeysHelp) shows: count rows.
// rc::kFunctionKeys unless a board's keyboard reaches them differently.
const rc::FunctionKey* functionKeys(size_t& count);
// The last line of the screensaver, telling the user how to wake the screen.
const char* wakeHint();
// The next pending key press, already mapped to a portable key.
bool pollKey(rc::KeyEvent& key);
int batteryPercent();  // -1 when unknown
bool externalPower();
// Returns at once; the tone plays in the background. hasSound: the board has
// a buzzer or speaker (default true); without one beep does nothing.
bool hasSound();
void beep(int freqHz, int ms);
// hasVolume: beeps can be made louder or softer (default false). setVolume
// takes a percent from ui/volume.h and applies to the next beep.
bool hasVolume();
void setVolume(uint8_t pct);
// A vibration motor. hasHaptic: the board has one; haptic buzzes it for ms
// and returns at once. Default: none, and haptic does nothing.
bool hasHaptic();
void haptic(int ms);
// The rc::Rotation index before any is saved. Default: 0.
uint8_t defaultRotation();
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
// A centre tap (Select) on a dark screen or the screensaver shows the 10 s
// peek at recent messages. False: keys do nothing there, only the wake key
// (ScreenOff) acts. Default true.
bool keysPeek();
// What hints call the key that opens a screen's commands (Post in the
// composer): "ctrl" by default (ui/menu_key.h).
const char32_t* menuKeyName();
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
// Text can be typed on the device (a keyboard or a touch keyboard). False:
// text fields open the canned replies instead (ui/widgets/canned_picker.h).
// Defaults to true (board_defaults.cpp); the X4 says false.
bool canType();
// Web config may send text for the device (the X4 only; spec
// 2026-10-30-cyberspace-x4-design.md §4.4). Defaults to false.
bool webCompose();
// The emoji key (Ctrl) while typing opens the screen's actions first; the
// same key again while they're open swaps them for the emoji tray (the M9).
// Default false: it opens the tray straight away.
bool emojiKeyOpensActions();
// Boards that sleep on battery and wake on a timer (the X4). Defaults: none.
bool sleepsOnBattery();
// A board whose panel takes one text size only (the Cardputer ADV's 240x135:
// Compact) sets out and returns true; the UI then uses it on every screen and
// hides the text-size choice. Default false.
bool fixedTextSize(rc::TextSize& out);
// False on a panel too small for Large text (the Wio Tracker L2): Large is
// not offered and becomes Comfortable. Default true.
bool hasLargeText();
// Battery volts, 0 when unknown. Default 0.
float batteryVolts();
// Deep sleep: wakes on the board's wake button, or after timerUs (0: no
// timer). Does not return. Default: a no-op.
void deepSleep(uint64_t timerUs);
// True when this boot is a deep-sleep timer wake. Default false.
bool wokeByTimer();
// Cuts the battery (a latch); does not return on boards that have one.
// Default: a no-op.
void powerOff();
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
