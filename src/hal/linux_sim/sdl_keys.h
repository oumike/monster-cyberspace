// SPDX-License-Identifier: GPL-3.0-or-later
// SDL key events as the evdev codes the CardputerZero's kernel sends, for
// rc::CardputerZeroKeys: the simulator's keyboard, and USB or Bluetooth
// keyboards in the CardputerZero's desktop window.
#pragma once

#include <SDL.h>

#include <cstdint>
#include <unordered_map>

#include "input/keymap_cardputer_zero.h"

namespace linuxhal {

// The evdev code for a host key (US layout), or 0.
uint16_t evdevCode(SDL_Keycode k, bool shift);

// Pairs each release with the code its press sent (shift may have changed in
// between). Main thread only.
class SdlKeys {
 public:
  // False for repeats (the keymap times its own) and keys with no code.
  bool translate(const SDL_KeyboardEvent& k, uint32_t nowMs, rc::ZeroKeyIn& out);

 private:
  std::unordered_map<int, uint16_t> down_;  // SDL scancode -> code sent on press
};

}  // namespace linuxhal
