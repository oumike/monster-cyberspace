#include <unity.h>

#include "net/ap_fallback.h"

using namespace rc;
using Act = ApFallback::Act;

void setUp() {}
void tearDown() {}

constexpr int64_t kMin = 60000;

void test_raises_after_five_minutes_without_wifi() {
  ApFallback f;
  TEST_ASSERT_TRUE(f.update(1000, false, false, 0) == Act::None);
  TEST_ASSERT_TRUE(f.update(1000 + 5 * kMin - 1, false, false, 0) == Act::None);
  TEST_ASSERT_TRUE(f.update(1000 + 5 * kMin, false, false, 0) == Act::Raise);
}

void test_connection_restarts_the_timer() {
  ApFallback f;
  f.update(0, false, false, 0);
  f.update(4 * kMin, true, false, 0);
  TEST_ASSERT_TRUE(f.update(8 * kMin, false, false, 0) == Act::None);
  TEST_ASSERT_TRUE(f.update(9 * kMin, false, false, 0) == Act::Raise);
}

void test_no_raise_while_ap_is_up() {
  ApFallback f;
  f.update(0, false, true, 0);
  TEST_ASSERT_TRUE(f.update(10 * kMin, false, true, 0) == Act::None);
}

void test_fallback_ap_drops_on_reconnect_without_clients() {
  ApFallback f;
  f.update(0, false, false, 0);
  TEST_ASSERT_TRUE(f.update(5 * kMin, false, false, 0) == Act::Raise);
  TEST_ASSERT_TRUE(f.update(6 * kMin, true, true, 1) == Act::None);  // someone is on the page
  TEST_ASSERT_TRUE(f.update(7 * kMin, true, true, 0) == Act::Drop);
  TEST_ASSERT_TRUE(f.update(8 * kMin, true, false, 0) == Act::None);
}

void test_manual_ap_is_never_dropped() {
  ApFallback f;
  f.update(0, true, false, 0);
  f.noteManual(kMin, true);
  TEST_ASSERT_TRUE(f.update(2 * kMin, true, true, 0) == Act::None);
}

void test_manual_off_restarts_the_timer() {
  ApFallback f;
  f.update(0, false, false, 0);
  TEST_ASSERT_TRUE(f.update(5 * kMin, false, false, 0) == Act::Raise);
  f.noteManual(6 * kMin, false);
  TEST_ASSERT_TRUE(f.update(10 * kMin, false, false, 0) == Act::None);
  TEST_ASSERT_TRUE(f.update(11 * kMin, false, false, 0) == Act::Raise);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_raises_after_five_minutes_without_wifi);
  RUN_TEST(test_connection_restarts_the_timer);
  RUN_TEST(test_no_raise_while_ap_is_up);
  RUN_TEST(test_fallback_ap_drops_on_reconnect_without_clients);
  RUN_TEST(test_manual_ap_is_never_dropped);
  RUN_TEST(test_manual_off_restarts_the_timer);
  return UNITY_END();
}
