#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_x4.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

void test_ladders_decode_measured_values() {
  // Idle, Back, Confirm, Left, Right as measured on an X4 (rhino-moshi x4.h).
  TEST_ASSERT_EQUAL_UINT8(0, x4ButtonMask(65520, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Back, x4ButtonMask(55780, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Confirm, x4ButtonMask(42710, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Left, x4ButtonMask(23750, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Right, x4ButtonMask(50, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Up, x4ButtonMask(65520, 35530, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Down, x4ButtonMask(65520, 60, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Power | kX4Up, x4ButtonMask(65520, 35530, true));
  // Boundaries: ranges[i+1] < raw <= ranges[i].
  TEST_ASSERT_EQUAL_UINT8(kX4Back, x4ButtonMask(60500, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(kX4Confirm, x4ButtonMask(49000, 65520, false));
  TEST_ASSERT_EQUAL_UINT8(0, x4ButtonMask(60501, 50501, false));
}

// Feeds `mask` every 5 ms from `from` to `to` and returns the events.
static int run(X4Keys& k, uint8_t mask, uint32_t from, uint32_t to, KeyEvent* out, int max) {
  int n = 0;
  for (uint32_t t = from; t <= to; t += 5) n += k.update(mask, t, out + n, max - n);
  return n;
}

void test_arrow_counts_after_debounce() {
  X4Keys k;
  KeyEvent e[8];
  TEST_ASSERT_EQUAL(0, run(k, kX4Down, 0, 45, e, 8));  // still bouncing
  TEST_ASSERT_EQUAL(1, run(k, kX4Down, 50, 55, e, 8));
  expectAction(e[0], Action::Down);
  TEST_ASSERT_EQUAL(0, run(k, 0, 60, 200, e, 8));
}

void test_short_glitch_is_ignored() {
  X4Keys k;
  KeyEvent e[8];
  TEST_ASSERT_EQUAL(0, run(k, kX4Up, 0, 20, e, 8));
  TEST_ASSERT_EQUAL(0, run(k, 0, 25, 300, e, 8));
}

void test_held_arrow_repeats() {
  X4Keys k;
  KeyEvent e[16];
  const int n = run(k, kX4Up, 0, 50 + kX4RepeatDelayMs + 2 * kX4RepeatMs, e, 16);
  TEST_ASSERT_EQUAL(4, n);  // the press and three repeats
  for (int i = 0; i < n; ++i) expectAction(e[i], Action::Up);
}

void test_confirm_tap_and_hold() {
  X4Keys k;
  KeyEvent e[8];
  TEST_ASSERT_EQUAL(0, run(k, kX4Confirm, 0, 200, e, 8));  // waits for release
  TEST_ASSERT_EQUAL(1, run(k, 0, 205, 300, e, 8));
  expectAction(e[0], Action::Select);

  X4Keys h;
  TEST_ASSERT_EQUAL(1, run(h, kX4Confirm, 0, 50 + kX4HoldMs, e, 8));
  expectAction(e[0], Action::Palette);
  TEST_ASSERT_EQUAL(0, run(h, kX4Confirm, 55 + kX4HoldMs, 2000, e, 8));
  TEST_ASSERT_EQUAL(0, run(h, 0, 2005, 2200, e, 8));  // nothing on release
}

void test_back_tap_and_hold() {
  X4Keys k;
  KeyEvent e[8];
  run(k, kX4Back, 0, 100, e, 8);
  TEST_ASSERT_EQUAL(1, run(k, 0, 105, 200, e, 8));
  expectAction(e[0], Action::Back);

  X4Keys h;
  TEST_ASSERT_EQUAL(1, run(h, kX4Back, 0, 50 + kX4HoldMs, e, 8));
  expectAction(e[0], Action::OpenHome);
}

void test_left_right_and_power() {
  X4Keys k;
  KeyEvent e[8];
  run(k, kX4Left, 0, 50, e, 8);
  expectAction(e[0], Action::Left);
  run(k, 0, 55, 200, e, 8);
  run(k, kX4Right, 205, 260, e, 8);
  expectAction(e[0], Action::Right);
  run(k, 0, 265, 400, e, 8);

  run(k, kX4Power, 405, 500, e, 8);
  TEST_ASSERT_EQUAL(1, run(k, 0, 505, 600, e, 8));
  expectAction(e[0], Action::ScreenOff);

  X4Keys h;
  TEST_ASSERT_EQUAL(0, run(h, kX4Power, 0, 50 + kX4PowerHoldMs - 10, e, 8));
  TEST_ASSERT_EQUAL(1, run(h, kX4Power, 45 + kX4PowerHoldMs, 60 + kX4PowerHoldMs, e, 8));
  expectAction(e[0], Action::PowerOff);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ladders_decode_measured_values);
  RUN_TEST(test_arrow_counts_after_debounce);
  RUN_TEST(test_short_glitch_is_ignored);
  RUN_TEST(test_held_arrow_repeats);
  RUN_TEST(test_confirm_tap_and_hold);
  RUN_TEST(test_back_tap_and_hold);
  RUN_TEST(test_left_right_and_power);
  return UNITY_END();
}
