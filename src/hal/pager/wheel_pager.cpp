#if defined(BOARD_PAGER)

#include "hal/pager/wheel_pager.h"

#include <Arduino.h>

#include <soc/gpio_reg.h>

#include <atomic>

#include "hal/pager/pins_pager.h"
#include "input/click_timer.h"
#include "input/rotary.h"

namespace wheel {
namespace {

using rc::Action;

// LilyGO's direction: a positive detent (camillia-mt's ScrollUp) is Up.
constexpr bool kPositiveIsUp = true;  // confirm on hardware (TESTING.md)
constexpr uint32_t kBootHoldMs = 600;

// The ISR only records line states; decoding happens in poll().
constexpr uint8_t kRing = 64;
uint8_t ring[kRing];
std::atomic<uint8_t> ringHead{0};
uint8_t ringTail = 0;

rc::Rotary rotary;
rc::ClickTimer click;
bool clickDown = false;
bool bootDown = false, bootHeld = false;
uint32_t bootMs = 0;

// Straight from the input register: the ISR runs from IRAM, digitalRead doesn't.
static_assert(pager::WHEEL_A >= 32 && pager::WHEEL_B >= 32, "lines() reads GPIO_IN1");
uint8_t IRAM_ATTR lines() {
  const uint32_t in = REG_READ(GPIO_IN1_REG);
  return (uint8_t)(((in >> (pager::WHEEL_B - 32)) & 1 ? 0 : 2) |
                   ((in >> (pager::WHEEL_A - 32)) & 1 ? 0 : 1));
}

void IRAM_ATTR onEdge() {
  const uint8_t h = ringHead.load(std::memory_order_relaxed);
  ring[h % kRing] = lines();
  ringHead.store((uint8_t)(h + 1), std::memory_order_release);
}

bool emit(rc::KeyEvent& key, Action a) {
  key = rc::KeyEvent{};
  key.action = a;
  return true;
}

}  // namespace

void begin() {
  for (int p : {pager::WHEEL_A, pager::WHEEL_B, pager::WHEEL_CLICK, pager::BOOT_BUTTON})
    pinMode(p, INPUT_PULLUP);
  rotary.reset(lines());
  attachInterrupt(digitalPinToInterrupt(pager::WHEEL_A), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pager::WHEEL_B), onEdge, CHANGE);
}

bool poll(rc::KeyEvent& key) {
  const uint32_t now = millis();

  const uint8_t h = ringHead.load(std::memory_order_acquire);
  // Too far behind to trust: start over from where the wheel is now.
  if ((uint8_t)(h - ringTail) > kRing) {
    ringTail = h;
    rotary.reset(lines());
  }
  for (; ringTail != h; ++ringTail) rotary.sample(ring[ringTail % kRing]);
  if (const int step = rotary.take()) {
    click.motion(now);
    return emit(key, (step > 0) == kPositiveIsUp ? Action::Up : Action::Down);
  }

  const bool down = digitalRead(pager::WHEEL_CLICK) == LOW;
  if (down != clickDown) {
    clickDown = down;
    if (down) click.press(now);
    else click.release(now);
  }
  if (Action a = click.poll(now); a != Action::None) return emit(key, a);

  const bool boot = digitalRead(pager::BOOT_BUTTON) == LOW;
  if (boot && !bootDown) {
    bootDown = true;
    bootHeld = false;
    bootMs = now;
  } else if (boot && !bootHeld && now - bootMs >= kBootHoldMs) {
    bootHeld = true;
    return emit(key, Action::OpenHome);
  } else if (!boot && bootDown) {
    bootDown = false;
    if (!bootHeld) return emit(key, Action::Back);
  }
  return false;
}

}  // namespace wheel

#endif
