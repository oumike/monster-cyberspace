// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(BOARD_CARDPUTER_ZERO) || defined(BOARD_LINUX_SIM)

#include "sdl_keys.h"

namespace linuxhal {

namespace {

// The sym-layer code that types c, from the launcher's built-in table.
uint16_t symCode(char c) {
  static constexpr struct {
    char c;
    uint16_t code;
  } kSym[] = {
      {'`', 69}, {'~', 55}, {'[', 74}, {'{', 76}, {']', 75}, {'}', 77}, {'\\', 86}, {'|', 89},
      {';', 79}, {':', 80}, {'\'', 81}, {'"', 82}, {',', 90}, {'<', 83}, {'.', 91}, {'>', 85},
      {'/', 92}, {'?', 93},
  };
  for (const auto& k : kSym) {
    if (k.c == c) return k.code;
  }
  return 0;
}

}  // namespace

uint16_t evdevCode(SDL_Keycode k, bool shift) {
  using namespace rc::zerokey;
  if (k >= SDLK_a && k <= SDLK_z) {
    static constexpr uint16_t kLetters[] = {kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM,
                                            kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ};
    return kLetters[k - SDLK_a];
  }
  if (k >= SDLK_1 && k <= SDLK_9) return uint16_t(k1 + (k - SDLK_1));
  if (k >= SDLK_F1 && k <= SDLK_F10) return uint16_t(kF1 + (k - SDLK_F1));
  switch (k) {
    case SDLK_0: return k0;
    case SDLK_MINUS: return kMinus;
    case SDLK_EQUALS: return kEqual;
    case SDLK_SPACE: return kSpace;
    case SDLK_ESCAPE: return kEsc;
    case SDLK_RETURN: return kEnter;
    case SDLK_KP_ENTER: return kKpEnter;
    case SDLK_TAB: return kTab;
    case SDLK_BACKSPACE: return kBackspace;
    case SDLK_DELETE: return kDelete;
    case SDLK_UP: return kUp;
    case SDLK_DOWN: return kDown;
    case SDLK_LEFT: return kLeft;
    case SDLK_RIGHT: return kRight;
    case SDLK_HOME: return kHome;
    case SDLK_END: return kEnd;
    case SDLK_PAGEUP: return kPageUp;
    case SDLK_PAGEDOWN: return kPageDown;
    case SDLK_F11: return kF11;
    case SDLK_F12: return kF12;
    case SDLK_LSHIFT: return kLeftShift;
    case SDLK_RSHIFT: return kRightShift;
    case SDLK_LALT: return kFn;
    case SDLK_RALT: return kCompose;
    case SDLK_BACKQUOTE: return symCode(shift ? '~' : '`');
    case SDLK_LEFTBRACKET: return symCode(shift ? '{' : '[');
    case SDLK_RIGHTBRACKET: return symCode(shift ? '}' : ']');
    case SDLK_BACKSLASH: return symCode(shift ? '|' : '\\');
    case SDLK_SEMICOLON: return symCode(shift ? ':' : ';');
    case SDLK_QUOTE: return symCode(shift ? '"' : '\'');
    case SDLK_COMMA: return symCode(shift ? '<' : ',');
    case SDLK_PERIOD: return symCode(shift ? '>' : '.');
    case SDLK_SLASH: return symCode(shift ? '?' : '/');
    default: return 0;
  }
}

bool SdlKeys::translate(const SDL_KeyboardEvent& k, uint32_t nowMs, rc::ZeroKeyIn& out) {
  if (k.repeat) return false;
  uint16_t code = 0;
  if (k.type == SDL_KEYDOWN) {
    code = evdevCode(k.keysym.sym, (k.keysym.mod & KMOD_SHIFT) != 0);
    if (!code) return false;
    down_[k.keysym.scancode] = code;
  } else {
    const auto it = down_.find(k.keysym.scancode);
    if (it == down_.end()) return false;
    code = it->second;
    down_.erase(it);
  }
  out = {code, uint8_t(k.type == SDL_KEYDOWN ? 1 : 0), nowMs, 0};
  return true;
}

}  // namespace linuxhal

#endif
