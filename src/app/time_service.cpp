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
#endif

#include "time/time_status.h"

namespace rc::timesvc {

namespace {

// The one place the process TZ changes. A web save may call it while the UI
// task runs localtime(); see the GPS time Task 6 report for that residual risk.
void applyTz(const std::string& tz) {
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

}  // namespace

#if RC_FAKE_API

std::mutex gMutex;  // guards gTzName, gTz and the TZ change
std::string gTzName, gTz;

void apply(const Settings& s) {
  std::lock_guard<std::mutex> lock(gMutex);
  gTzName = s.tzName;
  gTz = s.tz;
  applyTz(gTz);
}

// A fixed clock with no network: 2026-10-29 23:42 UTC.
void start(const Settings& s) {
  timeval tv{1793317320, 0};
  settimeofday(&tv, nullptr);
  apply(s);
}

void restartNtp() {}

void settingsChanged() {
  Settings s;
  settings::load(s);
  apply(s);
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> lock(gMutex);
  return {"fix, 7 of 11 satellites", "GPS, 2 min ago", zoneStatusText(gTzName, gTz), gTzName, gTz};
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

std::mutex gMutex;  // guards gTzName, gTz, gSnap and the TZ change
std::string gTzName, gTz;
Snapshot gSnap;

ClockPolicy gPolicy;  // the time task's only, after start()
NmeaParser gParser;

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

// Under gMutex.
void setZone(Snapshot& s) {
  s.zone = zoneStatusText(gTzName, gTz);
  s.tzName = gTzName;
  s.tz = gTz;
}

void rebuildSnapshot(int64_t nowMs) {
  const ClockStatus c = gPolicy.status();
  Snapshot s;
  s.gps = gpsStatusText(board::hasGps(), c, gParser.state(), nowMs);
  s.time = timeStatusText(c, nowMs);
  std::lock_guard<std::mutex> lock(gMutex);
  setZone(s);
  gSnap = std::move(s);
}

void tickPolicy(int64_t nowMs, bool& hadFix) {
  ClockInputs in;
  in.hasGps = board::hasGps();
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
    gTzName = s.tzName;
    gTz = s.tz;
    applyTz(gTz);
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

  if (xTaskCreatePinnedToCore(taskMain, "time", 4096, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("[time] task start failed");
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
    gTzName = s.tzName;
    gTz = s.tz;
    applyTz(gTz);
    setZone(gSnap);
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
