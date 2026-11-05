#include <unity.h>

#include "power/battery.h"
#include "power/battery_meter.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_first_reading_shows_at_once() {
  BatteryMeter m;
  TEST_ASSERT_TRUE(m.due(0));
  m.feed(73, 0);
  TEST_ASSERT_EQUAL(73, m.shown());
  TEST_ASSERT_FALSE(m.due(BatteryMeter::kSampleMs - 1));
  TEST_ASSERT_TRUE(m.due(BatteryMeter::kSampleMs));
}

void test_jitter_does_not_flicker() {
  BatteryMeter m;
  m.feed(50, 0);
  // Readings bouncing a few points either side never move the shown value.
  const int raw[] = {53, 47, 52, 48, 54, 46, 51, 49, 53, 47, 52, 48};
  int64_t t = 0;
  for (int r : raw) {
    t += BatteryMeter::kSampleMs;
    m.feed(r, t);
    TEST_ASSERT_EQUAL(50, m.shown());
  }
}

void test_follows_a_real_change() {
  BatteryMeter m;
  m.feed(80, 0);
  int64_t t = 0;
  for (int i = 0; i < 30; ++i) m.feed(60, t += BatteryMeter::kSampleMs);
  TEST_ASSERT_INT_WITHIN(1, 60, m.shown());
}

void test_unknown_after_several_misses() {
  BatteryMeter m;
  m.feed(40, 0);
  m.feed(-1, 2000);
  m.feed(-1, 4000);
  TEST_ASSERT_EQUAL(40, m.shown());
  m.feed(-1, 6000);
  TEST_ASSERT_EQUAL(-1, m.shown());
  m.feed(90, 8000);
  TEST_ASSERT_EQUAL(90, m.shown());
}

// The Tanmatsu's cell is read under load: badge-bsp's curve, 4.05 V full.
void test_tanmatsu_curve() {
  TEST_ASSERT_EQUAL(100, tanmatsuBatteryPercent(4.10f));
  TEST_ASSERT_EQUAL(100, tanmatsuBatteryPercent(4.05f));
  TEST_ASSERT_EQUAL(81, tanmatsuBatteryPercent(3.90f));
  TEST_ASSERT_EQUAL(59, tanmatsuBatteryPercent(3.75f));
  TEST_ASSERT_EQUAL(51, tanmatsuBatteryPercent(3.70f));
  TEST_ASSERT_EQUAL(0, tanmatsuBatteryPercent(3.30f));
  TEST_ASSERT_EQUAL(0, tanmatsuBatteryPercent(3.10f));
  TEST_ASSERT_EQUAL(-1, tanmatsuBatteryPercent(0.0f));
}

// Two VBAT reads count only when both are in the ADC's range and agree:
// one read can pair bytes of two samples.
void test_tanmatsu_vbat_reads_must_agree() {
  uint16_t mv = 0;
  TEST_ASSERT_TRUE(tanmatsuVbatAgree(3800, 3830, mv));
  TEST_ASSERT_EQUAL(3815, mv);
  TEST_ASSERT_FALSE(tanmatsuVbatAgree(3800, 3800 + 256, mv));
  TEST_ASSERT_FALSE(tanmatsuVbatAgree(2000, 2000, mv));
  TEST_ASSERT_FALSE(tanmatsuVbatAgree(5000, 5000, mv));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_reading_shows_at_once);
  RUN_TEST(test_jitter_does_not_flicker);
  RUN_TEST(test_follows_a_real_change);
  RUN_TEST(test_unknown_after_several_misses);
  RUN_TEST(test_tanmatsu_curve);
  RUN_TEST(test_tanmatsu_vbat_reads_must_agree);
  return UNITY_END();
}
