#include <unity.h>

#include <string>

#include "gps/nmea.h"
#include "time/clock_policy.h"
#include "time/time_status.h"

using namespace rc;

void setUp() {}
void tearDown() {}

constexpr int64_t H = 3600000, M = 60000, T = 1793039400000LL;

static GpsState gpsAt(int64_t atMs, int64_t epochMs, bool fix) {
  GpsState g;
  g.utcValid = true;
  g.fix = fix;
  g.utcAtMs = atMs;
  g.utcEpochMs = epochMs;
  if (fix) g.lastFixMs = atMs;
  return g;
}

static ClockInputs withGps(const GpsState* g, bool hasGps = true) {
  ClockInputs in;
  in.hasGps = hasGps;
  in.gps = g;
  return in;
}

static ClockInputs ntp(int64_t systemEpochMs) {
  ClockInputs in;
  in.ntpSynced = true;
  in.systemEpochMs = systemEpochMs;
  return in;
}

static int src(const ClockPolicy& p) { return static_cast<int>(p.status().source); }

void test_boot_from_rtc() {
  ClockPolicy p;
  ClockActions a = p.boot(0, true, 1793039400);
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.clockEpochMs == T);
  TEST_ASSERT_FALSE(a.writeRtc);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Rtc), src(p));
  TEST_ASSERT_TRUE(p.status().lastSyncMs == -1);
}

void test_boot_rtc_implausible() {
  ClockPolicy p;
  ClockActions a = p.boot(0, true, 946684800);
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::None), src(p));

  ClockPolicy q;
  a = q.boot(0, false, 1793039400);
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::None), src(q));
}

void test_gps_sets_unset_clock() {
  ClockPolicy p;
  p.boot(0, false, 0);
  GpsState g = gpsAt(800, T, true);
  ClockActions a = p.tick(1000, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.clockEpochMs == T + 200);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Gps), src(p));
  TEST_ASSERT_TRUE(p.status().lastSyncMs == 1000);
  TEST_ASSERT_TRUE(a.writeRtc);
  TEST_ASSERT_TRUE(a.rtcEpochSec == (T + 200) / 1000);
}

void test_ntp_beats_gps() {
  ClockPolicy p;
  p.boot(0, false, 0);
  ClockActions a = p.tick(0, ntp(T));
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_TRUE(a.writeRtc);
  TEST_ASSERT_TRUE(a.rtcEpochSec == T / 1000);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Ntp), src(p));

  GpsState g = gpsAt(30 * M, T + 30 * M, true);
  a = p.tick(30 * M, withGps(&g));
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Ntp), src(p));
}

void test_gps_after_ntp_stale() {
  ClockPolicy p;
  p.boot(0, false, 0);
  p.tick(0, ntp(T));
  GpsState g = gpsAt(H + 1, T + H + 1, true);
  ClockActions a = p.tick(H + 1, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.clockEpochMs == T + H + 1);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Gps), src(p));
}

void test_gps_implausible_ignored() {
  ClockPolicy p;
  p.boot(0, false, 0);
  GpsState g = gpsAt(800, 1136073600000LL, true);  // 2006-01-01
  ClockActions a = p.tick(1000, withGps(&g));
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_FALSE(a.writeRtc);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::None), src(p));
}

void test_gps_plausible_bounds() {
  ClockPolicy p;
  p.boot(0, false, 0);
  GpsState g = gpsAt(1000, kMaxPlausibleSec * 1000, true);  // 2100-01-01: excluded
  ClockActions a = p.tick(1000, withGps(&g));
  TEST_ASSERT_FALSE(a.setClock);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::None), src(p));

  ClockPolicy q;
  q.boot(0, false, 0);
  g = gpsAt(1000, kMinPlausibleSec * 1000, true);  // 2025-01-01: included
  a = q.tick(1000, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.clockEpochMs == kMinPlausibleSec * 1000);
}

void test_rtc_boot_counts_unsynced() {
  ClockPolicy p;
  p.boot(0, true, 1793039400);
  ClockActions a = p.tick(1000, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_TRUE(a.gpsOn);
  GpsState g = gpsAt(5000, T + 5000, true);
  a = p.tick(5000, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.clockEpochMs == T + 5000);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::Gps), src(p));
}

void test_rtc_write_throttled() {
  ClockPolicy p;
  p.boot(0, false, 0);
  GpsState g = gpsAt(0, T, true);
  ClockActions a = p.tick(0, withGps(&g));
  TEST_ASSERT_TRUE(a.writeRtc);
  a = p.tick(30 * M, ntp(T + 30 * M));
  TEST_ASSERT_FALSE(a.writeRtc);
  a = p.tick(61 * M, ntp(T + 61 * M));
  TEST_ASSERT_TRUE(a.writeRtc);
  TEST_ASSERT_TRUE(a.rtcEpochSec == (T + 61 * M) / 1000);
}

void test_gps_power_on_off() {
  ClockPolicy p;
  p.boot(0, false, 0);
  ClockActions a = p.tick(0, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_TRUE(a.gpsOn);
  TEST_ASSERT_TRUE(p.status().gpsOn);
  TEST_ASSERT_TRUE(p.status().searchStartMs == 0);

  a = p.tick(1000, withGps(nullptr));
  TEST_ASSERT_FALSE(a.gpsChanged);

  GpsState g = gpsAt(2000, T, true);
  a = p.tick(2000, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_FALSE(a.gpsOn);
  TEST_ASSERT_FALSE(p.status().gpsOn);

  GpsState empty;
  a = p.tick(2000 + 6 * H, withGps(&empty));
  TEST_ASSERT_FALSE(a.gpsChanged);
  a = p.tick(2000 + 6 * H + 1, withGps(&empty));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_TRUE(a.gpsOn);
}

void test_ntp_turns_gps_off() {
  ClockPolicy p;
  p.boot(0, false, 0);
  ClockActions a = p.tick(0, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsOn);
  ClockInputs in = ntp(T);
  in.hasGps = true;
  a = p.tick(5000, in);
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_FALSE(a.gpsOn);
}

void test_no_gps_hardware() {
  ClockPolicy p;
  p.boot(0, false, 0);
  for (int64_t t = 0; t <= 24 * H; t += 15 * M) {
    ClockActions a = p.tick(t, withGps(nullptr, false));
    TEST_ASSERT_FALSE(a.gpsOn);
    TEST_ASSERT_FALSE(a.gpsChanged);
    TEST_ASSERT_FALSE(p.status().gpsOn);
  }
}

void test_search_gives_up_and_retries() {
  const int64_t base = 5000000000LL;  // 58 days: beyond 32 bits
  ClockPolicy p;
  p.boot(base, false, 0);
  ClockActions a = p.tick(base, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_TRUE(a.gpsOn);
  a = p.tick(base + 15 * M - 1, withGps(nullptr));
  TEST_ASSERT_FALSE(a.gpsChanged);
  a = p.tick(base + 15 * M, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_FALSE(a.gpsOn);
  TEST_ASSERT_TRUE(a.gaveUp);
  a = p.tick(base + 15 * M + 59 * M, withGps(nullptr));
  TEST_ASSERT_FALSE(a.gpsChanged);
  TEST_ASSERT_FALSE(p.status().gpsOn);
  a = p.tick(base + 15 * M + H, withGps(nullptr));
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_TRUE(a.gpsOn);
  TEST_ASSERT_FALSE(a.gaveUp);
  TEST_ASSERT_TRUE(p.status().searchStartMs == base + 15 * M + H);
}

void test_gps_at_search_limit_not_gave_up() {
  ClockPolicy p;
  p.boot(0, false, 0);
  p.tick(0, withGps(nullptr));
  GpsState g = gpsAt(15 * M, T + 15 * M, true);
  ClockActions a = p.tick(15 * M, withGps(&g));
  TEST_ASSERT_TRUE(a.setClock);
  TEST_ASSERT_TRUE(a.gpsChanged);
  TEST_ASSERT_FALSE(a.gpsOn);
  TEST_ASSERT_FALSE(a.gaveUp);  // the time arrived; no retry wait
  a = p.tick(15 * M + 1000, withGps(&g));
  TEST_ASSERT_FALSE(a.gpsChanged);
}

void test_fresh_ntp_keeps_gps_off() {
  ClockPolicy p;
  p.boot(0, false, 0);
  ClockInputs in = ntp(T);
  in.hasGps = true;
  ClockActions a = p.tick(0, in);
  TEST_ASSERT_FALSE(a.gpsOn);
  TEST_ASSERT_FALSE(p.status().gpsOn);
}

void test_zone_status_text() {
  TEST_ASSERT_EQUAL_STRING("America/New_York",
                           zoneStatusText("America/New_York", "EST5EDT,M3.2.0,M11.1.0").c_str());
  TEST_ASSERT_EQUAL_STRING("UTC0 (custom)", zoneStatusText("", "UTC0").c_str());
}

void test_status_text() {
  ClockStatus c;
  GpsState g;
  TEST_ASSERT_EQUAL_STRING("none", gpsStatusText(false, c, g, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("off", gpsStatusText(true, c, g, 0).c_str());
  c.gpsOn = true;
  c.searchStartMs = 1000;
  TEST_ASSERT_EQUAL_STRING("searching 3:07", gpsStatusText(true, c, g, 188000).c_str());
  g.fix = true;
  g.satsUsed = 7;
  g.satsInView = 11;
  TEST_ASSERT_EQUAL_STRING("fix, 7 of 11 satellites", gpsStatusText(true, c, g, 188000).c_str());

  ClockStatus t;
  TEST_ASSERT_EQUAL_STRING("not set", timeStatusText(t, 0).c_str());
  t.source = TimeSource::Rtc;
  TEST_ASSERT_EQUAL_STRING("RTC, at boot", timeStatusText(t, 5 * H).c_str());
  t.source = TimeSource::Ntp;
  t.lastSyncMs = 1000;
  TEST_ASSERT_EQUAL_STRING("NTP, just now", timeStatusText(t, 1000 + 59 * 1000).c_str());
  t.source = TimeSource::Gps;
  TEST_ASSERT_EQUAL_STRING("GPS, 12 min ago", timeStatusText(t, 1000 + 12 * M + 30000).c_str());
  t.source = TimeSource::Ntp;
  TEST_ASSERT_EQUAL_STRING("NTP, 3 h ago", timeStatusText(t, 1000 + 3 * H + 10 * M).c_str());
  TEST_ASSERT_EQUAL_STRING("NTP, 4 d ago", timeStatusText(t, 1000 + 4 * 24 * H + 5 * H).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_from_rtc);
  RUN_TEST(test_boot_rtc_implausible);
  RUN_TEST(test_gps_sets_unset_clock);
  RUN_TEST(test_ntp_beats_gps);
  RUN_TEST(test_gps_after_ntp_stale);
  RUN_TEST(test_gps_implausible_ignored);
  RUN_TEST(test_gps_plausible_bounds);
  RUN_TEST(test_rtc_boot_counts_unsynced);
  RUN_TEST(test_rtc_write_throttled);
  RUN_TEST(test_gps_power_on_off);
  RUN_TEST(test_ntp_turns_gps_off);
  RUN_TEST(test_no_gps_hardware);
  RUN_TEST(test_search_gives_up_and_retries);
  RUN_TEST(test_gps_at_search_limit_not_gave_up);
  RUN_TEST(test_fresh_ntp_keeps_gps_off);
  RUN_TEST(test_zone_status_text);
  RUN_TEST(test_status_text);
  return UNITY_END();
}
