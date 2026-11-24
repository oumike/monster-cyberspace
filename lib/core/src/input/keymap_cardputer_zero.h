// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "input/function_keys.h"
#include "input/key_event.h"

namespace rc {

// Linux evdev key codes (linux/input-event-codes.h) the CardputerZero's
// keyboard sends; spelled out so lib/core needs no Linux headers.
namespace zerokey {
enum : uint16_t {
  kEsc = 1, k1 = 2, k2, k3, k4, k5, k6, k7, k8, k9, k0, kMinus = 12, kEqual = 13,
  kBackspace = 14, kTab = 15,
  kQ = 16, kW, kE, kR, kT, kY, kU, kI, kO, kP,
  kEnter = 28, kLeftCtrl = 29,
  kA = 30, kS, kD, kF, kG, kH, kJ, kK, kL,
  kLeftShift = 42,
  kZ = 44, kX, kC, kV, kB, kN, kM,
  kRightShift = 54, kLeftAlt = 56, kSpace = 57,
  kF1 = 59, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10,
  kF11 = 87, kF12 = 88, kKpEnter = 96, kSysRq = 99,
  kHome = 102, kUp = 103, kPageUp = 104, kLeft = 105, kRight = 106, kEnd = 107,
  kDown = 108, kPageDown = 109, kInsert = 110, kDelete = 111,
  kMute = 113, kVolumeDown = 114, kVolumeUp = 115,
  kCompose = 127, kHelp = 138, kBrightnessDown = 224, kBrightnessUp = 225,
  kFn = 0x1d0,
};
}  // namespace zerokey

// Held arrows and Backspace repeat after kZeroRepeatDelayMs, every kZeroRepeatMs;
// the keymap times them and ignores the kernel's own repeats. Esc held
// kZeroQuitHoldMs quits to the launcher.
constexpr uint32_t kZeroRepeatDelayMs = 500, kZeroRepeatMs = 70, kZeroQuitHoldMs = 3000;

// ZeroKeyIn::mods: the modes /sys/bus/i2c/devices/1-0034/modifier_state
// reports (one-shot and locked shift may never arrive as KEY_LEFTSHIFT).
enum : uint8_t { kZeroModShift = 1, kZeroModFn = 2 };

struct ZeroKeyIn {
  uint16_t code;
  uint8_t value;  // 0 up, 1 down, 2 the kernel's repeat
  uint32_t ms;
  uint8_t mods;   // kZeroMod* bits; 0 if unknown
};

// Keys the HAL handles itself rather than the screens.
enum class ZeroSystemKey : uint8_t { None, BrightnessUp, BrightnessDown, VolumeUp, VolumeDown, Mute };

// F10's table: the CardputerZero reaches F1-F10 as fn+1...0.
constexpr FunctionKey kCardputerZeroFunctionKeys[] = {
    {U"F1", U"home", Action::OpenHome},         {U"F2", U"messages", Action::OpenCirc},
    {U"F3", U"c-mail", Action::OpenCmail},      {U"F4", U"feed", Action::OpenFeed},
    {U"F5", U"guilds", Action::OpenMap},        {U"F6", U"profile", Action::OpenProfile},
    {U"F7", U"settings", Action::OpenSettings}, {U"F10", U"navigational help", Action::KeysHelp},
    {U"Ctrl", U"actions, emoji", Action::Emoji}, {U"Tab", U"menus, post", Action::Palette},
};
constexpr size_t kCardputerZeroFunctionKeyCount = std::size(kCardputerZeroFunctionKeys);

// The M5Stack CardputerZero's keyboard, as raw evdev events from its kernel
// TCA8418 driver, which applies shift/sym/fn (holding, latching, locking)
// itself (spec 2026-11-16-cyberspace-cardputer-zero-design.md §2, §5).
// Letters, digits, space, - and = follow the US layout with Shift; the sym
// layer's reused codes go through a table (the launcher's built-in one, or
// the system keymap file through loadSymMap), whatever Shift says. The fn
// layer arrives mapped (arrows, Home, F-keys …); should a raw key come with
// fn active (the simulator's Left Alt), the keymap applies the printed fn
// layer itself. Ctrl tapped alone is the M9's Ctrl (Action::Emoji: the
// actions, then the emoji tray, while typing; the palette elsewhere); keys
// pressed while it is held type nothing. Esc is Back on release, or quits
// once held kZeroQuitHoldMs;
// Enter Select, Tab the palette, Home Home, PgUp/PgDn Up/Down, Delete
// Backspace, Help (fn+H) and F10 the help.
class CardputerZeroKeys {
 public:
  CardputerZeroKeys();
  // An evdev EV_KEY event; true and out set when it makes a key.
  bool feed(const ZeroKeyIn& in, KeyEvent& out);
  // A held key's repeat, and the Esc hold timer; true and out set on a repeat.
  bool poll(uint32_t ms, KeyEvent& out);
  bool quitRequested() const { return quit_; }
  // How long Esc has been held at ms, toward kZeroQuitHoldMs; 0 when it is
  // up or the hold already quit.
  uint32_t escHeldMs(uint32_t ms) const { return escDown_ && !quit_ ? ms - escMs_ : 0; }
  // The last brightness or volume key, once.
  ZeroSystemKey takeSystemKey();
  // Replaces the sym table with tca8418_keypad_m5stack_keymap.map's
  // "keycode N = keysym" lines (# comments; lines or keysyms it can't read are
  // skipped). False, keeping the current table, when it has no usable line.
  bool loadSymMap(const char* text);

 private:
  static constexpr size_t kSymMax = 64;
  struct SymEntry {
    uint16_t code;
    char32_t ch;
  };
  bool press(const ZeroKeyIn& in, KeyEvent& out);
  char32_t symChar(uint16_t code) const;

  SymEntry sym_[kSymMax];
  size_t symCount_ = 0;
  bool leftShift_ = false, rightShift_ = false, fn_ = false;
  bool ctrl_ = false, ctrlUsed_ = false;  // Ctrl held; another key pressed with it
  bool escDown_ = false;
  uint32_t escMs_ = 0;
  bool quit_ = false;
  ZeroSystemKey system_ = ZeroSystemKey::None;
  uint16_t heldCode_ = 0;
  Action heldAction_ = Action::None;
  uint32_t nextRepeatMs_ = 0;
};

}  // namespace rc
