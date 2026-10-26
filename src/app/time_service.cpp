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

#include "gps/nmea.h"
#include "hal/board.h"
#include "time/clock_policy.h"
#include "time/time_status.h"
#endif

namespace rc::timesvc {

namespace {

void applyTz(const std::string& tz) {
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

}  // namespace

#if RC_FAKE_API

void start(const Settings& s) { applyTz(s.tz); }

void restartNtp() {}

void settingsChanged() {
  Settings s;
  settings::load(s);
  applyTz(s.tz);
}

Snapshot snapshot() {
  return {"fix, 7 of 11 satellites", "GPS, 2 min ago", "America/New_York (auto)"};
}

#else

namespace {

constexpr uint32_t kPollMs = 100;
constexpr int64_t kTickMs = 1000;
constexpr int64_t kSearchLimitMs = 15 * 60000;
constexpr size_t kChunk = 256;

// esp_sntp_setservername keeps the pointer, so the name must outlive SNTP.
std::string gNtpServer = "pool.ntp.org";
std::atomic<bool> gNtpSynced{false};

std::mutex gMutex;  // guards gTz and gSnap
std::string gTz;
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

void rebuildSnapshot(int64_t nowMs) {
  const ClockStatus c = gPolicy.status();
  Snapshot s;
  s.gps = gpsStatusText(board::hasGps(), c, gParser.state(), nowMs);
  s.time = timeStatusText(c, nowMs);
  std::lock_guard<std::mutex> lock(gMutex);
  s.zone = gTz + " (manual)";
  gSnap = std::move(s);
}

void tickPolicy(int64_t nowMs, bool& hadFix) {
  const ClockStatus before = gPolicy.status();
  ClockInputs in;
  in.hasGps = board::hasGps();
  in.autoZone = false;  // zone modes arrive with Task 6
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
      gParser.reset();
      hadFix = false;
      // The policy gives up a search after 15 min; a sync this tick means
      // the GPS simply is no longer needed.
      const bool gaveUp = before.gpsOn && nowMs - before.searchStartMs >= kSearchLimitMs &&
                          !a.setClock && !in.ntpSynced;
      Serial.println(gaveUp ? "[gps] off: no fix in 15 min, retry in 1 h" : "[gps] off");
    }
  }

  const GpsState& g = gParser.state();
  if (g.fix && !hadFix)
    Serial.printf("[gps] fix, %d of %d satellites\n", g.satsUsed, g.satsInView);
  hadFix = g.fix;
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
  applyTz(s.tz);
  {
    std::lock_guard<std::mutex> lock(gMutex);
    gTz = s.tz;
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
  esp_sntp_setservername(0, gNtpServer.c_str());
  esp_sntp_setservername(1, "time.google.com");
  sntp_set_time_sync_notification_cb(onNtpSync);

  xTaskCreatePinnedToCore(taskMain, "time", 4096, nullptr, 1, nullptr, 0);
}

void restartNtp() {
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_init();
}

void settingsChanged() {
  Settings s;
  settings::load(s);
  applyTz(s.tz);
  std::lock_guard<std::mutex> lock(gMutex);
  gTz = s.tz;
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> lock(gMutex);
  return gSnap;
}

#endif

}  // namespace rc::timesvc
