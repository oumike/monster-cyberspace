#include "time/time_status.h"

#include <cstdio>

namespace rc {

std::string gpsStatusText(bool hasGps, const ClockStatus& c, const GpsState& g, int64_t nowMs) {
  if (!hasGps) return "none";
  if (!c.gpsOn) return "off";
  char buf[40];
  if (g.fix) {
    snprintf(buf, sizeof buf, "fix, %d of %d satellites", g.satsUsed, g.satsInView);
  } else {
    int64_t sec = c.searchStartMs >= 0 && nowMs > c.searchStartMs ? (nowMs - c.searchStartMs) / 1000 : 0;
    snprintf(buf, sizeof buf, "searching %d:%02d", static_cast<int>(sec / 60), static_cast<int>(sec % 60));
  }
  return buf;
}

std::string timeStatusText(const ClockStatus& c, int64_t nowMs) {
  switch (c.source) {
    case TimeSource::None:
      return "not set";
    case TimeSource::Rtc:
      return "RTC, at boot";
    default:
      break;
  }
  const char* name = c.source == TimeSource::Gps ? "GPS" : "NTP";
  const int64_t age = c.lastSyncMs >= 0 && nowMs > c.lastSyncMs ? nowMs - c.lastSyncMs : 0;
  constexpr int64_t kMin = 60000, kHour = 60 * kMin, kDay = 24 * kHour;
  char buf[32];
  if (age < kMin) {
    snprintf(buf, sizeof buf, "%s, just now", name);
  } else if (age < kHour) {
    snprintf(buf, sizeof buf, "%s, %d min ago", name, static_cast<int>(age / kMin));
  } else if (age < 2 * kDay) {
    snprintf(buf, sizeof buf, "%s, %d h ago", name, static_cast<int>(age / kHour));
  } else {
    snprintf(buf, sizeof buf, "%s, %d d ago", name, static_cast<int>(age / kDay));
  }
  return buf;
}

}  // namespace rc
