#pragma once

#include <cstdint>
#include <string>

#include "gps/nmea.h"
#include "time/clock_policy.h"

// One-line status text for the GPS and the clock, as shown on Settings.
namespace rc {

// "none", "off", "searching m:ss" or "fix, N of M satellites".
std::string gpsStatusText(bool hasGps, const ClockStatus& c, const GpsState& g, int64_t nowMs);

// "not set", "RTC, at boot", or the source and age of the last sync,
// e.g. "NTP, just now", "GPS, 12 min ago", "NTP, 3 h ago", "NTP, 4 d ago".
std::string timeStatusText(const ClockStatus& c, int64_t nowMs);

// The zone's IANA name, or "<posix> (custom)" when it has none.
std::string zoneStatusText(const std::string& tzName, const std::string& posix);

}  // namespace rc
