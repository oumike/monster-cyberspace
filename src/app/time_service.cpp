#include "app/time_service.h"

#include <Arduino.h>
#include <sys/time.h>

#include <cstdlib>
#include <ctime>
#include <mutex>

#if !RC_FAKE_API
#include <esp_netif.h>
#include <esp_sntp.h>
#include <esp_timer.h>

#include <atomic>
#include <cstring>

#include "gps/nmea.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "time/clock_policy.h"
#include "time/time_status.h"
#include "tz/tz_data.h"  // the 1.16 MB zone blob: include it nowhere else
#endif

#include "tz/tz_lookup.h"

namespace rc::timesvc {

namespace {

// The one place the process TZ changes. The time task may call it while the UI
// task runs localtime(); see the Task 6 report for that residual risk.
void applyTz(TzMode mode, const std::string& manualTz, const std::string& autoPosix) {
  setenv("TZ", effectiveTz(mode, manualTz, autoPosix).c_str(), 1);
  tzset();
}

}  // namespace

#if RC_FAKE_API

void start(const Settings& s) { applyTz(s.tzMode, s.tz, s.tzAutoPosix); }

void restartNtp() {}

void settingsChanged() {
  Settings s;
  settings::load(s);
  applyTz(s.tzMode, s.tz, s.tzAutoPosix);
}

Snapshot snapshot() {
  return {"fix, 7 of 11 satellites", "GPS, 2 min ago", "America/New_York (auto)"};
}

#else

namespace {

constexpr uint32_t kPollMs = 100;
constexpr int64_t kTickMs = 1000;
constexpr size_t kChunk = 256;

// esp_sntp_setservername keeps the pointer, so the name lives in a fixed
// buffer that changes only while SNTP is stopped, under gNtpMutex.
char gNtpServer[64] = "pool.ntp.org";
std::mutex gNtpMutex;  // guards gNtpServer and SNTP start/stop
std::atomic<bool> gNtpSynced{false};

std::mutex gMutex;  // guards the zone fields, gSnap and the TZ change
std::string gTz;    // the manual zone
TzMode gTzMode = TzMode::Manual;
std::string gTzAutoName, gTzAutoPosix;
Snapshot gSnap;
std::atomic<bool> gTrackerReset{false};  // settingsChanged() switched to Automatic

ClockPolicy gPolicy;  // the time task's only, after start()
NmeaParser gParser;
TzTracker gTracker;
int64_t gLastPosMs = -1;  // posAtMs of the last position given to the tracker

int64_t monoMs() { return esp_timer_get_time() / 1000; }

int64_t systemEpochMs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

void setSystemClock(int64_t epochMs) {
  timeval tv;
  tv.tv_sec = time_t(epochMs / 1000);
  tv.tv_usec = suseconds_t((epochMs % 1000) * 1000);
  settimeofday(&tv, nullptr);
}

void onNtpSync(timeval*) { gNtpSynced = true; }

void rebuildSnapshot(int64_t nowMs) {
  const ClockStatus c = gPolicy.status();
  Snapshot s;
  s.gps = gpsStatusText(board::hasGps(), c, gParser.state(), nowMs);
  s.time = timeStatusText(c, nowMs);
  std::lock_guard<std::mutex> lock(gMutex);
  s.zone = zoneStatusText(gTzMode, gTzAutoName, gTz);
  gSnap = std::move(s);
}

bool autoZone() {
  std::lock_guard<std::mutex> lock(gMutex);
  return gTzMode == TzMode::Automatic;
}

// Looks the zone up for a newly parsed position (never a fix without one).
void trackZone() {
  if (gTrackerReset.exchange(false)) gTracker.reset();
  const GpsState& g = gParser.state();
  if (!g.fix || g.posAtMs < 0 || g.posAtMs == gLastPosMs) return;
  gLastPosMs = g.posAtMs;
  TzZone z;
  if (!gTracker.update(kTzData, kTzDataSize, g.lat, g.lon, z)) return;
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gTzMode != TzMode::Automatic || z.name == gTzAutoName) return;
    gTzAutoName = z.name;
    gTzAutoPosix = z.posix;
    applyTz(gTzMode, gTz, gTzAutoPosix);
  }
  settings::saveTzAuto(z.name, z.posix);
  Serial.printf("[tz] zone %s\n", z.name.c_str());
}

void tickPolicy(int64_t nowMs, bool& hadFix) {
  const bool zoneAuto = autoZone();
  ClockInputs in;
  in.hasGps = board::hasGps();
  in.autoZone = zoneAuto;
  in.ntpSynced = gNtpSynced.exchange(false);
  in.systemEpochMs = systemEpochMs();
  in.gps = &gParser.state();
  if (in.ntpSynced) Serial.println("[time] NTP sync");

  const ClockActions a = gPolicy.tick(nowMs, in);
  if (a.setClock) {
    setSystemClock(a.clockEpochMs);
    Serial.println("[time] set from GPS");  // tick only sets the clock from GPS
  }
  if (a.writeRtc && board::rtcWrite(a.rtcEpochSec)) Serial.println("[time] RTC written");
  if (a.gpsChanged) {
    board::gpsPower(a.gpsOn);
    if (a.gpsOn) {
      Serial.println("[gps] on");
    } else {
      Serial.println(a.gaveUp ? "[gps] off: no fix in 15 min, retry in 1 h" : "[gps] off");
    }
  }

  const GpsState& g = gParser.state();
  if (g.fix && !hadFix)
    Serial.printf("[gps] fix, %d of %d satellites\n", g.satsUsed, g.satsInView);
  hadFix = g.fix;
  if (zoneAuto) trackZone();
  if (a.gpsChanged && !a.gpsOn) {
    gParser.reset();
    hadFix = false;
  }
}

void taskMain(void*) {
  uint8_t buf[kChunk];
  int64_t lastTick = -kTickMs;
  bool hadFix = false;
  for (;;) {
    size_t n;
    while ((n = board::gpsRead(buf, sizeof buf)) > 0) gParser.feed(buf, n, monoMs());
    const int64_t now = monoMs();
    if (now - lastTick >= kTickMs) {
      lastTick = now;
      tickPolicy(now, hadFix);
      rebuildSnapshot(now);
    }
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
  }
}

}  // namespace

void start(const Settings& s) {
  {
    std::lock_guard<std::mutex> lock(gMutex);
    gTz = s.tz;
    gTzMode = s.tzMode;
    gTzAutoName = s.tzAutoName;
    gTzAutoPosix = s.tzAutoPosix;
    applyTz(gTzMode, gTz, gTzAutoPosix);
  }

  int64_t rtcSec = 0;
  const bool rtcOk = board::rtcRead(rtcSec);
  const ClockActions a = gPolicy.boot(monoMs(), rtcOk, rtcSec);
  if (a.setClock) {
    setSystemClock(a.clockEpochMs);
    Serial.println("[time] set from RTC");
  }
  rebuildSnapshot(monoMs());

  // The SNTP calls go through the lwIP core, which esp_netif_init brings up.
  esp_netif_init();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  {
    std::lock_guard<std::mutex> lock(gNtpMutex);
    strlcpy(gNtpServer, s.ntpServer.c_str(), sizeof gNtpServer);
    esp_sntp_setservername(0, gNtpServer);
  }
  esp_sntp_setservername(1, "time.google.com");
  sntp_set_time_sync_notification_cb(onNtpSync);

  xTaskCreatePinnedToCore(taskMain, "time", 4096, nullptr, 1, nullptr, 0);
}

void restartNtp() {
  std::lock_guard<std::mutex> lock(gNtpMutex);
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_init();
}

void settingsChanged() {
  Settings s;
  settings::load(s);
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (s.tzMode == TzMode::Automatic && gTzMode != TzMode::Automatic) gTrackerReset = true;
    gTz = s.tz;
    gTzMode = s.tzMode;
    gTzAutoName = s.tzAutoName;
    gTzAutoPosix = s.tzAutoPosix;
    applyTz(gTzMode, gTz, gTzAutoPosix);
  }
  std::lock_guard<std::mutex> lock(gNtpMutex);
  if (s.ntpServer == gNtpServer) return;
  if (esp_sntp_enabled()) esp_sntp_stop();
  strlcpy(gNtpServer, s.ntpServer.c_str(), sizeof gNtpServer);
  esp_sntp_setservername(0, gNtpServer);
  if (wifimgr::connected()) esp_sntp_init();
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> lock(gMutex);
  return gSnap;
}

#endif

}  // namespace rc::timesvc
