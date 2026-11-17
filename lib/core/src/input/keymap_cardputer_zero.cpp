// SPDX-License-Identifier: GPL-3.0-or-later
#include "input/keymap_cardputer_zero.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rc {
namespace {

using namespace zerokey;

// The launcher's built-in table (CardputerZero/launcher,
// cp0_keyboard_keymap.c), for when the system keymap file is missing.
constexpr struct {
  uint16_t code;
  char ch;
} kBuiltinSym[] = {
    {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'}, {51, '&'},
    {52, '*'}, {53, '('}, {94, ')'}, {55, '~'}, {69, '`'}, {70, '_'}, {71, '-'},
    {72, '+'}, {73, '='}, {74, '['}, {75, ']'}, {76, '{'}, {77, '}'}, {79, ';'},
    {80, ':'}, {81, '\''}, {82, '"'}, {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'},
    {90, ','}, {91, '.'}, {92, '/'}, {93, '?'},
};

// X keysym names for printable ASCII (xkbcommon's keysym names).
constexpr struct {
  const char* name;
  char ch;
} kKeysymNames[] = {
    {"space", ' '},       {"exclam", '!'},       {"quotedbl", '"'},     {"numbersign", '#'},
    {"dollar", '$'},      {"percent", '%'},      {"ampersand", '&'},    {"apostrophe", '\''},
    {"quoteright", '\''}, {"parenleft", '('},    {"parenright", ')'},   {"asterisk", '*'},
    {"plus", '+'},        {"comma", ','},        {"minus", '-'},        {"period", '.'},
    {"slash", '/'},       {"colon", ':'},        {"semicolon", ';'},    {"less", '<'},
    {"equal", '='},       {"greater", '>'},      {"question", '?'},     {"at", '@'},
    {"bracketleft", '['}, {"backslash", '\\'},   {"bracketright", ']'}, {"asciicircum", '^'},
    {"underscore", '_'},  {"grave", '`'},        {"quoteleft", '`'},    {"braceleft", '{'},
    {"bar", '|'},         {"braceright", '}'},   {"asciitilde", '~'},
};

bool isHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// A keysym name's character, or 0: single letters and digits, the ASCII
// names above, and Unicode keysyms ("U20AC").
char32_t keysymChar(const char* name) {
  const size_t n = std::strlen(name);
  if (n == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z') ||
                 (name[0] >= '0' && name[0] <= '9'))) {
    return char32_t(name[0]);
  }
  for (const auto& k : kKeysymNames) {
    if (std::strcmp(k.name, name) == 0) return char32_t(k.ch);
  }
  if (n >= 5 && n <= 7 && name[0] == 'U') {
    for (size_t i = 1; i < n; ++i) {
      if (!isHex(name[i])) return 0;
    }
    const unsigned long cp = std::strtoul(name + 1, nullptr, 16);
    if (cp >= 0x20 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF)) return char32_t(cp);
  }
  return 0;
}

// The US layout's plain and shifted characters for the keys that type.
struct Pair {
  char plain, shifted;
};

Pair usChar(uint16_t code) {
  static constexpr char kTop[] = "qwertyuiop";
  static constexpr char kHome[] = "asdfghjkl";
  static constexpr char kBottom[] = "zxcvbnm";
  static constexpr char kDigits[] = "1234567890";
  static constexpr char kShiftedDigits[] = "!@#$%^&*()";
  if (code >= kQ && code <= kP) return {kTop[code - kQ], char(kTop[code - kQ] - 32)};
  if (code >= kA && code <= kL) return {kHome[code - kA], char(kHome[code - kA] - 32)};
  if (code >= kZ && code <= kM) return {kBottom[code - kZ], char(kBottom[code - kZ] - 32)};
  if (code >= k1 && code <= k0) return {kDigits[code - k1], kShiftedDigits[code - k1]};
  switch (code) {
    case kSpace: return {' ', ' '};
    case kMinus: return {'-', '_'};
    case kEqual: return {'=', '+'};
    default: return {0, 0};
  }
}

bool isModifier(uint16_t code) {
  return code == kLeftShift || code == kRightShift || code == kFn || code == kCompose ||
         code == kLeftCtrl || code == kLeftAlt;
}

bool repeats(Action a) {
  return a == Action::Backspace || a == Action::Up || a == Action::Down || a == Action::Left ||
         a == Action::Right;
}

KeyEvent make(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

// The printed fn layer on a raw key; Action::None when it has none.
Action fnAction(uint16_t code, ZeroSystemKey& system) {
  if (code >= k1 && code <= k0) return functionKeyAction(code - k1 + 1);
  switch (code) {
    case kF: return Action::Up;
    case kX: return Action::Down;
    case kZ: return Action::Left;
    case kC: return Action::Right;
    case kK: return Action::OpenHome;
    case kL: return Action::Up;
    case kM: return Action::Down;
    case kBackspace: return Action::Backspace;
    case kH: return Action::KeysHelp;
    case kU: system = ZeroSystemKey::BrightnessDown; return Action::None;
    case kI: system = ZeroSystemKey::BrightnessUp; return Action::None;
    case kS: system = ZeroSystemKey::VolumeDown; return Action::None;
    case kD: system = ZeroSystemKey::VolumeUp; return Action::None;
    default: return Action::None;
  }
}

// Keys that mean the same whatever the modifiers.
Action fixedAction(uint16_t code, ZeroSystemKey& system) {
  if (code >= kF1 && code <= kF10) return functionKeyAction(code - kF1 + 1);
  switch (code) {
    case kEnter: case kKpEnter: return Action::Select;
    case kTab: return Action::Palette;
    case kBackspace: case kDelete: return Action::Backspace;
    case kUp: case kPageUp: return Action::Up;
    case kDown: case kPageDown: return Action::Down;
    case kLeft: return Action::Left;
    case kRight: return Action::Right;
    case kHome: return Action::OpenHome;
    case kHelp: return Action::KeysHelp;
    case kVolumeUp: system = ZeroSystemKey::VolumeUp; return Action::None;
    case kVolumeDown: system = ZeroSystemKey::VolumeDown; return Action::None;
    case kMute: system = ZeroSystemKey::Mute; return Action::None;
    case kBrightnessUp: system = ZeroSystemKey::BrightnessUp; return Action::None;
    case kBrightnessDown: system = ZeroSystemKey::BrightnessDown; return Action::None;
    default: return Action::None;
  }
}

}  // namespace

CardputerZeroKeys::CardputerZeroKeys() {
  for (const auto& s : kBuiltinSym) sym_[symCount_++] = {s.code, char32_t(s.ch)};
}

char32_t CardputerZeroKeys::symChar(uint16_t code) const {
  for (size_t i = 0; i < symCount_; ++i) {
    if (sym_[i].code == code) return sym_[i].ch;
  }
  return 0;
}

bool CardputerZeroKeys::loadSymMap(const char* text) {
  if (!text) return false;
  SymEntry parsed[kSymMax];
  size_t count = 0;
  while (*text && count < kSymMax) {
    const char* end = std::strchr(text, '\n');
    const size_t len = end ? size_t(end - text) : std::strlen(text);
    char line[256];
    const size_t n = len < sizeof line - 1 ? len : sizeof line - 1;
    std::memcpy(line, text, n);
    line[n] = 0;
    text += len + (end ? 1 : 0);
    if (char* hash = std::strchr(line, '#')) *hash = 0;
    unsigned code = 0;
    char name[64] = {};
    if (std::sscanf(line, " keycode %u = %63s", &code, name) != 2 || code > 0xffff) continue;
    const char32_t ch = keysymChar(name);
    if (ch) parsed[count++] = {uint16_t(code), ch};
  }
  if (!count) return false;
  for (size_t i = 0; i < count; ++i) sym_[i] = parsed[i];
  symCount_ = count;
  return true;
}

ZeroSystemKey CardputerZeroKeys::takeSystemKey() {
  const ZeroSystemKey k = system_;
  system_ = ZeroSystemKey::None;
  return k;
}

bool CardputerZeroKeys::press(const ZeroKeyIn& in, KeyEvent& out) {
  const bool shift = leftShift_ || rightShift_ || (in.mods & kZeroModShift);
  const bool fn = fn_ || (in.mods & kZeroModFn);
  ZeroSystemKey system = ZeroSystemKey::None;
  Action a = fixedAction(in.code, system);
  if (a == Action::None && system == ZeroSystemKey::None && fn) a = fnAction(in.code, system);
  if (system != ZeroSystemKey::None) {
    system_ = system;
    return false;
  }
  if (a != Action::None) {
    out = make(a);
    if (repeats(a)) {
      heldCode_ = in.code;
      heldAction_ = a;
      nextRepeatMs_ = in.ms + kZeroRepeatDelayMs;
    }
    return true;
  }
  if (fn) return false;
  char32_t c = symChar(in.code);
  if (!c) {
    const Pair p = usChar(in.code);
    c = char32_t(shift ? p.shifted : p.plain);
  }
  if (!c) return false;
  out = make(Action::Char);
  out.ch = c;
  return true;
}

bool CardputerZeroKeys::feed(const ZeroKeyIn& in, KeyEvent& out) {
  if (escDown_ && in.ms - escMs_ >= kZeroQuitHoldMs) quit_ = true;
  const bool down = in.value == 1, up = in.value == 0;
  switch (in.code) {
    case kLeftShift: if (!down && !up) return false; leftShift_ = down; return false;
    case kRightShift: if (!down && !up) return false; rightShift_ = down; return false;
    case kFn: if (!down && !up) return false; fn_ = down; return false;
    default: break;
  }
  if (in.code == kEsc) {
    if (down && !escDown_) {
      escDown_ = true;
      escMs_ = in.ms;
      return false;
    }
    if (up && escDown_) {
      escDown_ = false;
      if (quit_) return false;
      out = make(Action::Back);
      return true;
    }
    return false;
  }
  if (up) {
    if (in.code == heldCode_) heldCode_ = 0;
    return false;
  }
  if (!down || isModifier(in.code)) return false;
  return press(in, out);
}

bool CardputerZeroKeys::poll(uint32_t ms, KeyEvent& out) {
  if (escDown_ && ms - escMs_ >= kZeroQuitHoldMs) quit_ = true;
  if (!heldCode_ || (int32_t)(ms - nextRepeatMs_) < 0) return false;
  nextRepeatMs_ = ms + kZeroRepeatMs;
  out = make(heldAction_);
  return true;
}

}  // namespace rc
