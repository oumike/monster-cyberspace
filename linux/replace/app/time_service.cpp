// SPDX-License-Identifier: GPL-3.0-or-later
// The clock is the system's (the OS keeps it with NTP); this only applies the
// zone and reports it (spec section 4.2). No SNTP, GPS or RTC code, and the
// demo build doesn't set a fake time: that would change the host's clock.
#include "app/time_service.h"

#include <Arduino.h>

#include <cstdlib>
#include <ctime>
#include <mutex>

#include "time/time_status.h"

namespace rc::timesvc {

namespace {

std::mutex gMutex;  // guards gTzName, gTz and the TZ change
std::string gTzName, gTz;

// The one place the process TZ changes.
void apply(const Settings& s) {
  std::lock_guard<std::mutex> lock(gMutex);
  gTzName = s.tzName;
  gTz = s.tz;
  setenv("TZ", gTz.c_str(), 1);
  tzset();
}

}  // namespace

void start(const Settings& s) {
  apply(s);
  Serial.println("[time] system clock");
}

void restartNtp() {}

void settingsChanged() {
  Settings s;
  settings::load(s);
  apply(s);
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> lock(gMutex);
  return {"none", "system", zoneStatusText(gTzName, gTz), gTzName, gTz};
}

}  // namespace rc::timesvc
