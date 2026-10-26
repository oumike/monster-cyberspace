#pragma once

#include <cstdint>
#include <string>

#include "gps/nmea.h"
#include "time/clock_policy.h"
#include "tz/tz_lookup.h"

// One-line status text for the GPS and the clock, as shown on Settings.
namespace rc {

// "none", "off", "searching m:ss" or "fix, N of M satellites".
std::string gpsStatusText(bool hasGps, const ClockStatus& c, const GpsState& g, int64_t nowMs);

// "not set", "RTC, at boot", or the source and age of the last sync,
// e.g. "NTP, just now", "GPS, 12 min ago", "NTP, 3 h ago", "NTP, 4 d ago".
std::string timeStatusText(const ClockStatus& c, int64_t nowMs);

// Manual: "<manualTz> (manual)"; Automatic with a zone: "<autoName> (auto)";
// Automatic before the first lookup: "<manualTz> (auto, no fix yet)".
std::string zoneStatusText(TzMode mode, const std::string& autoName, const std::string& manualTz);

}  // namespace rc
