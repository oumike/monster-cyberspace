#if defined(BOARD_TDECK)

#include "hal/tdeck/trackball_tdeck.h"

#include <Arduino.h>

#include <atomic>

#include "hal/tdeck/pins_tdeck.h"
#include "input/click_timer.h"

namespace trackball {
namespace {

using rc::Action;

constexpr uint32_t kMoveMs = 45;  // camillia's move debounce

std::atomic<uint16_t> pulses[4];  // Up, Down, Left, Right
constexpr Action kDir[4] = {Action::Up, Action::Down, Action::Left, Action::Right};

void IRAM_ATTR onUp() { pulses[0].fetch_add(1, std::memory_order_relaxed); }
void IRAM_ATTR onDown() { pulses[1].fetch_add(1, std::memory_order_relaxed); }
void IRAM_ATTR onLeft() { pulses[2].fetch_add(1, std::memory_order_relaxed); }
void IRAM_ATTR onRight() { pulses[3].fetch_add(1, std::memory_order_relaxed); }

rc::ClickTimer click;
bool clickDown = false;
uint32_t lastMoveMs = 0;

}  // namespace

void begin() {
  const int pins[4] = {tdeck::TB_UP, tdeck::TB_DOWN, tdeck::TB_LEFT, tdeck::TB_RIGHT};
  void (*isrs[4])() = {onUp, onDown, onLeft, onRight};
  for (int i = 0; i < 4; ++i) {
    pinMode(pins[i], INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pins[i]), isrs[i], FALLING);
  }
  pinMode(tdeck::TB_CLICK, INPUT_PULLUP);
}

bool poll(rc::KeyEvent& key) {
  const uint32_t now = millis();
  const bool down = digitalRead(tdeck::TB_CLICK) == LOW;
  if (down != clickDown) {
    clickDown = down;
    if (down) click.press(now);
    else click.release(now);
  }

  if (now - lastMoveMs >= kMoveMs) {
    // Take every pending pulse; the direction rolled most wins.
    uint16_t n[4];
    for (int i = 0; i < 4; ++i) n[i] = pulses[i].exchange(0, std::memory_order_relaxed);
    int best = -1;
    for (int i = 0; i < 4; ++i)
      if (n[i] && (best < 0 || n[i] > n[best])) best = i;
    if (best >= 0) {
      lastMoveMs = now;
      click.motion(now);
      key = rc::KeyEvent{};
      key.action = kDir[best];
      return true;
    }
  }

  Action a = click.poll(now);
  if (a == Action::None) return false;
  key = rc::KeyEvent{};
  key.action = a;
  return true;
}

}  // namespace trackball

#endif
