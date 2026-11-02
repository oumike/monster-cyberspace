#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// The Cyber Deck Plus's gamepad (Control: Gamepad (Bottom) 1.0): a left half
// on the TCA9555 at 0x22 and a right half on 0x26, both all-input.
//
// The D-pad drives the same four actions as the Core's own front buttons, so
// either input steers. Back is L1 (left half) and B (right half); A is a
// second Select, like the Core's own. X, Y, R1, R2 and the FN keys are left
// unmapped.
//
// A and B mirror the Core buttons exactly: a tap is Back (B, L1) or Select
// (A), and holding one for kCyberDeckHoldMs is OpenHome (B, L1) or ScreenOff
// (A), so the hold never also counts as a tap.

// Held directions repeat after this delay, then at this interval.
constexpr uint32_t kCyberDeckRepeatDelayMs = 500, kCyberDeckRepeatMs = 120;
// Holding Back or Select turns the screen off (A) or goes Home (B, L1).
constexpr uint32_t kCyberDeckHoldMs = 1000;

constexpr uint8_t kCyberDeckUp = 1 << 5, kCyberDeckDown = 1 << 2, kCyberDeckLeft = 1 << 4,
                  kCyberDeckRight = 1 << 3,  // left half
    kCyberDeckL1 = 1 << 6, kCyberDeckL2 = 1 << 7, kCyberDeckFnL1 = 1 << 0, kCyberDeckFnL2 = 1 << 1,
                  kCyberDeckA = 1 << 3, kCyberDeckB = 1 << 4, kCyberDeckX = 1 << 2,
                  kCyberDeckY = 1 << 5,  // right half
    kCyberDeckR1 = 1 << 1, kCyberDeckR2 = 1 << 0, kCyberDeckFnR1 = 1 << 7, kCyberDeckFnR2 = 1 << 6;

// Arrows on press, repeating while held. L1 and B emit Back and A emits Select,
// all on release so a held button can't repeat them; a release must read twice,
// which absorbs contact bounce.
class CyberDeckGamepad {
 public:
  // Feeds one read of both halves; writes up to max events, returns how many.
  int update(uint8_t left, uint8_t right, uint32_t ms, KeyEvent* out, int max);

 private:
  uint8_t down_[2] = {}, releasing_[2] = {}, held_[2] = {};
  uint32_t pressMs_[2][8] = {}, nextMs_[2][8] = {};
};

}  // namespace rc
