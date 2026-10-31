#include <unity.h>

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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_reading_shows_at_once);
  RUN_TEST(test_jitter_does_not_flicker);
  RUN_TEST(test_follows_a_real_change);
  RUN_TEST(test_unknown_after_several_misses);
  return UNITY_END();
}
