#pragma once

#include <string>

#include "store/settings.h"

// The clock: the RTC at boot, SNTP after each Wi-Fi connect, and the GPS when
// both are stale (spec §4). The fake builds keep their fixed clock and report
// canned status lines.
namespace rc::timesvc {

struct Snapshot {
  std::string gps, time, zone;  // ready-made status lines
};

// Applies the TZ, sets the clock from the RTC, configures SNTP and starts the
// "time" task. SNTP itself starts on the first restartNtp().
void start(const Settings& s);
// Called on every Wi-Fi station connect.
void restartNtp();
// Reloads the time settings from NVS and reapplies them.
void settingsChanged();
Snapshot snapshot();

}  // namespace rc::timesvc
