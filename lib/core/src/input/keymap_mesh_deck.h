#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// A Shift, Sym or Alt tap holds for this long, waiting for the key it modifies.
constexpr uint32_t kMeshDeckModifierMs = 1500;
// Held Backspace, Alt arrows and d-pad arrows repeat after
// kMeshDeckRepeatDelayMs, every kMeshDeckRepeatMs.
constexpr uint32_t kMeshDeckRepeatDelayMs = 500, kMeshDeckRepeatMs = 120;
// Holding Select turns the screen off (and on), holding L1 goes Home.
constexpr uint32_t kMeshDeckHoldMs = 1000;

// A key on the Attaky keyboard module: two 5x5 matrices, half 0 the left.
constexpr uint8_t meshDeckKeyPos(int half, int row, int col) {
  return static_cast<uint8_t>(half * 25 + row * 5 + col);
}

// The Mesh Deck keyboard (layout from platy-aprs's mesh_deck_io.cpp):
//   left:  1 2 3 4 5 / q w e r t / a s d f g / Shift z x c v / Lag Tab Alt , Space
//   right: 6 7 8 9 0 / y u i o p / h j k l Del / b n m - Enter / - . Mic Sym Fn
//
// Shift, Sym and Alt (Fn is a second Alt) work held, for every key while
// down, or as a one-shot tap for the next key. Shift upper-cases letters and
// gives the US number-row symbols; Sym is platy's punctuation layer.
// Del is Backspace, Enter Select, Tab the palette, Mic the emoji tray while
// typing, Sym+Enter the symbol table. Alt chords: W/A/S/D arrows,
// Del Back, Enter the palette, H Home, C cIRC, M C-Mail, F Feed.
class MeshDeckKeys {
 public:
  KeyEvent key(uint8_t pos, bool pressed, uint32_t ms);
  // A held Backspace's or Alt arrow's repeats; None otherwise.
  KeyEvent poll(uint32_t ms);

 private:
  uint8_t held_ = 0;   // modifier bits down
  uint8_t armed_ = 0;  // one-shot modifier bits
  uint32_t armedMs_ = 0;
  uint8_t repeatKey_ = 0xFF;
  Action repeatAction_ = Action::None;
  uint32_t repeatNextMs_ = 0;
};

// The front buttons, pressed bits as the HAL reads them (AW9523 port 0, with
// the power button's bit replaced by BOOT).
constexpr uint8_t kPadDown = 1 << 0, kPadLeft = 1 << 1, kPadSelect = 1 << 2, kPadRight = 1 << 3,
                  kPadUp = 1 << 4, kPadL1 = 1 << 5, kPadR2 = 1 << 6, kPadBoot = 1 << 7;

// Arrows on press, repeating while held. Select: tap Select, hold ScreenOff.
// L1: tap Back, hold Home. R2 and BOOT: ScreenOff on press. A press counts at
// once; a release must read twice, which absorbs contact bounce.
class MeshDeckPad {
 public:
  // Feeds one read; writes up to max events, returns how many.
  int update(uint8_t pressed, uint32_t ms, KeyEvent* out, int max);

 private:
  uint8_t down_ = 0, releasing_ = 0, held_ = 0;
  uint32_t pressMs_[8] = {}, nextMs_[8] = {};
};

}  // namespace rc
