#pragma once

#include <cstdint>

#include "gps/nmea.h"

// Decides where the clock comes from (RTC at boot, NTP, GPS), when to write
// the RTC, and when the GPS needs power. It has no clock of its own: nowMs is
// a monotonic millisecond count passed in by the caller.
namespace rc {

enum class TimeSource : uint8_t { None, Rtc, Gps, Ntp };

constexpr int64_t kMinPlausibleSec = 1735689600;  // 2025-01-01
constexpr int64_t kMaxPlausibleSec = 4102444800;  // 2100-01-01, exclusive

struct ClockInputs {
  bool hasGps = false;
  bool ntpSynced = false;       // the SNTP callback fired since the last tick
  int64_t systemEpochMs = 0;    // the system clock now
  const GpsState* gps = nullptr;
};

struct ClockActions {
  bool setClock = false;
  int64_t clockEpochMs = 0;
  bool writeRtc = false;
  int64_t rtcEpochSec = 0;
  bool gpsChanged = false;
  bool gpsOn = false;
  bool gaveUp = false;  // the GPS went off after a 15 min search; retry in 1 h
};

struct ClockStatus {
  TimeSource source = TimeSource::None;
  int64_t lastSyncMs = -1;  // monotonic; NTP or GPS only
  bool gpsOn = false;
  int64_t searchStartMs = -1;
};

class ClockPolicy {
 public:
  ClockActions boot(int64_t nowMs, bool rtcOk, int64_t rtcEpochSec);
  ClockActions tick(int64_t nowMs, const ClockInputs& in);
  ClockStatus status() const { return status_; }

 private:
  bool gpsNeeded(int64_t nowMs, const ClockInputs& in) const;
  void maybeWriteRtc(int64_t nowMs, int64_t epochMs, ClockActions& a);

  ClockStatus status_;
  int64_t lastNtpMs_ = -1;
  int64_t lastGpsSampleMs_ = -1;  // utcAtMs of the latest GPS sample used
  int64_t lastRtcWriteMs_ = -1;
  int64_t retryAtMs_ = -1;        // after a failed search, no search before this
};

}  // namespace rc
