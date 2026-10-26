#include "time/clock_policy.h"

namespace rc {

namespace {

constexpr int64_t kHourMs = 3600000;
constexpr int64_t kResyncMs = kHourMs;           // GPS may correct a sync this old
constexpr int64_t kRtcWriteGapMs = kHourMs;
constexpr int64_t kStaleMs = 6 * kHourMs;        // sync or position older than this needs the GPS
constexpr int64_t kSearchLimitMs = 15 * 60000;
constexpr int64_t kRetryWaitMs = kHourMs;

bool plausibleSec(int64_t sec) { return sec >= kMinPlausibleSec && sec <= kMaxPlausibleSec; }

bool olderThan(int64_t atMs, int64_t nowMs, int64_t ageMs) {
  return atMs < 0 || nowMs - atMs > ageMs;
}

}  // namespace

ClockActions ClockPolicy::boot(int64_t, bool rtcOk, int64_t rtcEpochSec) {
  ClockActions a;
  if (rtcOk && plausibleSec(rtcEpochSec)) {
    a.setClock = true;
    a.clockEpochMs = rtcEpochSec * 1000;
    status_.source = TimeSource::Rtc;
  }
  return a;
}

void ClockPolicy::maybeWriteRtc(int64_t nowMs, int64_t epochMs, ClockActions& a) {
  if (lastRtcWriteMs_ >= 0 && nowMs - lastRtcWriteMs_ < kRtcWriteGapMs) return;
  if (!plausibleSec(epochMs / 1000)) return;
  a.writeRtc = true;
  a.rtcEpochSec = epochMs / 1000;
  lastRtcWriteMs_ = nowMs;
}

bool ClockPolicy::gpsNeeded(int64_t nowMs, const ClockInputs& in) const {
  if (!in.hasGps) return false;
  if (olderThan(status_.lastSyncMs, nowMs, kStaleMs)) return true;
  return in.autoZone && olderThan(lastFixMs_, nowMs, kStaleMs);
}

ClockActions ClockPolicy::tick(int64_t nowMs, const ClockInputs& in) {
  ClockActions a;

  if (in.ntpSynced) {
    status_.source = TimeSource::Ntp;
    status_.lastSyncMs = nowMs;
    lastNtpMs_ = nowMs;
    maybeWriteRtc(nowMs, in.systemEpochMs, a);
  }

  const GpsState* g = in.gps;
  if (g) {
    if (g->lastFixMs > lastFixMs_) lastFixMs_ = g->lastFixMs;
    if (g->utcValid && g->utcAtMs > lastGpsSampleMs_) {
      const int64_t epochMs = g->utcEpochMs + (nowMs - g->utcAtMs);
      // An RTC boot leaves lastSyncMs at -1, so GPS may still set the clock.
      const bool allowed = olderThan(status_.lastSyncMs, nowMs, kResyncMs) &&
                           olderThan(lastNtpMs_, nowMs, kResyncMs);
      if (allowed && plausibleSec(epochMs / 1000)) {
        lastGpsSampleMs_ = g->utcAtMs;
        a.setClock = true;
        a.clockEpochMs = epochMs;
        status_.source = TimeSource::Gps;
        status_.lastSyncMs = nowMs;
        maybeWriteRtc(nowMs, epochMs, a);
      }
    }
  }

  const bool needed = gpsNeeded(nowMs, in);
  if (status_.gpsOn) {
    if (!needed) {
      status_.gpsOn = false;
      status_.searchStartMs = -1;
      a.gpsChanged = true;
    } else if (nowMs - status_.searchStartMs >= kSearchLimitMs) {
      status_.gpsOn = false;
      status_.searchStartMs = -1;
      retryAtMs_ = nowMs + kRetryWaitMs;
      a.gpsChanged = true;
    }
  } else if (needed && (retryAtMs_ < 0 || nowMs >= retryAtMs_)) {
    status_.gpsOn = true;
    status_.searchStartMs = nowMs;
    retryAtMs_ = -1;
    a.gpsChanged = true;
  }
  a.gpsOn = status_.gpsOn;
  return a;
}

}  // namespace rc
