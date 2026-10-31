#include <unity.h>

#include "power/power_policy.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectStep(PowerStep want, PowerStep got) {
  TEST_ASSERT_EQUAL(static_cast<int>(want), static_cast<int>(got));
}

void test_usb_stays_live() {
  PowerPolicy p;
  p.begin(false, 0);
  expectStep(PowerStep::Stay, p.step(10'000'000, true, 4.1f, 300));
}

void test_battery_sleeps_after_idle() {
  PowerPolicy p;
  p.begin(false, 0);
  expectStep(PowerStep::Stay, p.step(299'999, false, 3.9f, 300));
  expectStep(PowerStep::Sleep, p.step(300'000, false, 3.9f, 300));
  p.input(300'000);
  expectStep(PowerStep::Stay, p.step(400'000, false, 3.9f, 300));
}

void test_timed_wake_sleeps_once_refreshed() {
  PowerPolicy p;
  p.begin(true, 0);
  expectStep(PowerStep::Stay, p.step(5'000, false, 3.9f, 300));
  p.refreshed(6'000);
  expectStep(PowerStep::Sleep, p.step(6'000, false, 3.9f, 300));
}

void test_timed_wake_gives_up() {
  PowerPolicy p;
  p.begin(true, 0);
  expectStep(PowerStep::Stay, p.step(kTimedWakeMaxMs - 1, false, 3.9f, 300));
  expectStep(PowerStep::Sleep, p.step(kTimedWakeMaxMs, false, 3.9f, 300));
}

void test_button_turns_timed_wake_into_full_wake() {
  PowerPolicy p;
  p.begin(true, 0);
  p.input(1'000);
  p.refreshed(2'000);
  expectStep(PowerStep::Stay, p.step(kTimedWakeMaxMs + 5'000, false, 3.9f, 300));
}

void test_timed_wake_on_usb_stays() {
  PowerPolicy p;
  p.begin(true, 0);
  p.refreshed(1'000);
  expectStep(PowerStep::Stay, p.step(2'000, true, 4.2f, 300));
}

void test_critical_battery_powers_off_after_it_holds() {
  PowerPolicy p;
  p.begin(false, 0);
  expectStep(PowerStep::Stay, p.step(1'000, false, 3.30f, 300));
  expectStep(PowerStep::Stay, p.step(1'000 + kCriticalHoldMs - 1, false, 3.30f, 300));
  expectStep(PowerStep::PowerOff, p.step(1'000 + kCriticalHoldMs, false, 3.30f, 300));
}

void test_critical_dip_recovers() {
  PowerPolicy p;
  p.begin(false, 0);
  p.step(1'000, false, 3.30f, 300);
  p.step(2'000, false, 3.60f, 300);
  expectStep(PowerStep::Stay, p.step(2'000 + kCriticalHoldMs, false, 3.30f, 300));
  expectStep(PowerStep::Stay, p.step(3'000, true, 3.30f, 300));  // charging
  expectStep(PowerStep::Stay, p.step(3'000, false, 0.0f, 300));  // unread
}

void test_sleep_timer() {
  TEST_ASSERT_EQUAL_UINT64(0, wakeTimerUs(0));
  TEST_ASSERT_EQUAL_UINT64(15ull * 60 * 1000000, wakeTimerUs(15));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_usb_stays_live);
  RUN_TEST(test_battery_sleeps_after_idle);
  RUN_TEST(test_timed_wake_sleeps_once_refreshed);
  RUN_TEST(test_timed_wake_gives_up);
  RUN_TEST(test_button_turns_timed_wake_into_full_wake);
  RUN_TEST(test_timed_wake_on_usb_stays);
  RUN_TEST(test_critical_battery_powers_off_after_it_holds);
  RUN_TEST(test_critical_dip_recovers);
  RUN_TEST(test_sleep_timer);
  return UNITY_END();
}
