#include "input/keymap_x4.h"

namespace rc {
namespace {

constexpr int kLadder1[] = {60500, 49000, 33000, 12000, -1};
constexpr int kLadder2[] = {50500, 18000, -1};

int decode(int raw, const int* ranges, int count) {
  for (int i = 0; i < count; ++i)
    if (raw > ranges[i + 1] && raw <= ranges[i]) return i;
  return -1;
}

constexpr int kBack = 0, kConfirm = 1, kPower = 6;

bool isArrow(int b) { return b >= 2 && b <= 5; }

Action arrow(int b) {
  switch (b) {
    case 2: return Action::Left;
    case 3: return Action::Right;
    case 4: return Action::Up;
    default: return Action::Down;
  }
}

}  // namespace

uint8_t x4ButtonMask(int adc1, int adc2, bool powerDown) {
  uint8_t m = 0;
  const int b1 = decode(adc1, kLadder1, 4);
  if (b1 >= 0) m |= uint8_t(1 << b1);
  const int b2 = decode(adc2, kLadder2, 2);
  if (b2 >= 0) m |= uint8_t(1 << (b2 + 4));
  if (powerDown) m |= kX4Power;
  return m;
}

int X4Keys::update(uint8_t raw, uint32_t ms, KeyEvent* out, int max) {
  int n = 0;
  auto emit = [&](Action a) {
    if (n < max) out[n++] = KeyEvent{a};
  };
  for (int b = 0; b < 7; ++b) {
    const uint8_t bit = uint8_t(1 << b);
    if ((raw ^ raw_) & bit) rawMs_[b] = ms;
    const bool r = raw & bit, d = down_ & bit;
    if (r != d && ms - rawMs_[b] >= kX4DebounceMs) {
      if (r) {
        down_ |= bit;
        pressMs_[b] = ms;
        heldFired_ &= uint8_t(~bit);
        if (isArrow(b)) {
          emit(arrow(b));
          nextMs_[b] = ms + kX4RepeatDelayMs;
        }
      } else {
        down_ &= uint8_t(~bit);
        if (!(heldFired_ & bit)) {
          if (b == kConfirm) emit(Action::Select);
          else if (b == kBack) emit(Action::Back);
          else if (b == kPower) emit(Action::ScreenOff);
        }
      }
      continue;
    }
    if (!d) continue;
    if (isArrow(b)) {
      if (int32_t(ms - nextMs_[b]) >= 0) {
        emit(arrow(b));
        nextMs_[b] = ms + kX4RepeatMs;
      }
    } else if (!(heldFired_ & bit)) {
      const uint32_t hold = b == kPower ? kX4PowerHoldMs : kX4HoldMs;
      if (ms - pressMs_[b] >= hold) {
        heldFired_ |= bit;
        emit(b == kConfirm ? Action::Palette : b == kBack ? Action::OpenHome : Action::PowerOff);
      }
    }
  }
  raw_ = raw;
  return n;
}

}  // namespace rc
