#include "app/power_service.h"

#include <Arduino.h>

#include "app/app_state.h"
#include "hal/board.h"

namespace rc::power_service {
namespace {

PowerPolicy policy;
bool on = false;
// A timed wake counts as refreshed once the connection has been live this
// long: the open screen and the counts have come in and been drawn.
constexpr uint32_t kSettleMs = 15'000;
uint32_t liveSince = 0;

}  // namespace

void begin() {
  on = board::sleepsOnBattery();
  if (!on) return;
  policy.begin(board::wokeByTimer(), millis());
  Serial.printf("[power] %s\n", policy.timedWake() ? "timed wake" : "awake");
}

void input() {
  if (on) policy.input(millis());
}

PowerStep step() {
  if (!on) return PowerStep::Stay;
  const uint32_t now = millis();
  if (policy.timedWake()) {
    if (app().conn != 2) liveSince = 0;
    else if (!liveSince) liveSince = now ? now : 1;
    else if (now - liveSince >= kSettleMs) policy.refreshed(now);
  }
  return policy.step(now, board::externalPower(), board::batteryVolts(), app().cfg.idleSecs);
}

void sleep() {
  Serial.printf("[power] sleeping, waking in %u min\n", unsigned(app().cfg.wakeMins));
  board::deepSleep(wakeTimerUs(app().cfg.wakeMins));
}

void powerOff() {
  Serial.println("[power] off");
  board::powerOff();
}

}  // namespace rc::power_service
