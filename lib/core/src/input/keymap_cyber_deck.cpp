#include "input/keymap_cyber_deck.h"

namespace rc {
namespace {

KeyEvent action(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

// Only the left half carries the D-pad. Matching on the bit alone would make
// the right half's A/B/X/Y steer too: A is 1<<3, the same bit as Right, so a
// press would fire Right before the release sent Select.
Action arrow(int half, int b) {
  if (half != 0) return Action::None;
  switch (1 << b) {
    case kCyberDeckUp: return Action::Up;
    case kCyberDeckDown: return Action::Down;
    case kCyberDeckLeft: return Action::Left;
    case kCyberDeckRight: return Action::Right;
    default: return Action::None;
  }
}

// The Core's L1 is Back (hold: Home) and its Select is Select (hold: ScreenOff).
// The gamepad's B and A are the same two buttons, on either half's L1 and on A.
bool isBack(int half, uint8_t m) {
  return (half == 0 && m == kCyberDeckL1) || (half == 1 && m == kCyberDeckB);
}
bool isSelect(int half, uint8_t m) { return half == 1 && m == kCyberDeckA; }

}  // namespace

int CyberDeckGamepad::update(uint8_t left, uint8_t right, uint32_t ms, KeyEvent* out, int max) {
  const uint8_t pressed[2] = {left, right};
  int n = 0;
  auto emit = [&](Action a) {
    if (n < max) out[n++] = action(a);
  };
  for (int h = 0; h < 2; ++h) {
    for (int b = 0; b < 8; ++b) {
      const uint8_t m = uint8_t(1 << b);
      const Action dir = arrow(h, b);
      const bool tap = isBack(h, m) || isSelect(h, m);
      if (pressed[h] & m) {
        releasing_[h] &= uint8_t(~m);
        if (!(down_[h] & m)) {
          down_[h] |= m;
          held_[h] &= uint8_t(~m);
          pressMs_[h][b] = ms;
          nextMs_[h][b] = ms + kCyberDeckRepeatDelayMs;
          if (dir != Action::None) emit(dir);
        } else if (dir != Action::None) {
          if ((int32_t)(ms - nextMs_[h][b]) >= 0) {
            nextMs_[h][b] = ms + kCyberDeckRepeatMs;
            emit(dir);
          }
        } else if (tap && !(held_[h] & m) && ms - pressMs_[h][b] >= kCyberDeckHoldMs) {
          // Held: the hold action replaces the tap, as on the Core.
          held_[h] |= m;
          emit(isSelect(h, m) ? Action::ScreenOff : Action::OpenHome);
        }
      } else if (down_[h] & m) {
        if (!(releasing_[h] & m)) {
          releasing_[h] |= m;
          continue;
        }
        releasing_[h] &= uint8_t(~m);
        down_[h] &= uint8_t(~m);
        if (tap && !(held_[h] & m)) emit(isSelect(h, m) ? Action::Select : Action::Back);
        held_[h] &= uint8_t(~m);
      }
    }
  }
  return n;
}

}  // namespace rc
