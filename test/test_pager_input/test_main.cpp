#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_pager.h"
#include "input/rotary.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL((uint32_t)c, (uint32_t)e.ch);
}

void test_pager_letters() {
  PagerKeys k;
  expectChar(k.key(1, true, 0), 'q');
  expectChar(k.key(10, true, 0), 'p');
  expectChar(k.key(11, true, 0), 'a');
  expectChar(k.key(19, true, 0), 'l');
  expectChar(k.key(22, true, 0), 'z');
  expectChar(k.key(28, true, 0), 'm');
  expectChar(k.key(31, true, 0), ' ');
}

void test_pager_release_ignored() {
  PagerKeys k;
  expectAction(k.key(1, false, 0), Action::None);
  expectAction(k.key(0, true, 0), Action::None);
  expectAction(k.key(32, true, 0), Action::None);
}

void test_pager_shift_one_shot() {
  PagerKeys k;
  expectAction(k.key(29, true, 0), Action::None);
  expectAction(k.key(29, false, 10), Action::None);
  expectChar(k.key(1, true, 20), 'Q');
  expectChar(k.key(1, true, 30), 'q');
}

void test_pager_sym() {
  PagerKeys k;
  k.key(21, true, 0);
  expectChar(k.key(1, true, 10), '1');
  k.key(21, true, 20);
  expectChar(k.key(10, true, 30), '0');
  k.key(21, true, 40);
  expectChar(k.key(19, true, 50), '@');
  k.key(21, true, 60);
  expectChar(k.key(28, true, 70), '.');
  k.key(21, true, 80);
  expectChar(k.key(25, true, 90), '?');
}

void test_pager_modifier_toggles_off() {
  PagerKeys k;
  k.key(29, true, 0);
  k.key(29, true, 10);
  expectChar(k.key(1, true, 20), 'q');
}

void test_pager_modifier_times_out() {
  PagerKeys k;
  k.key(29, true, 0);
  expectChar(k.key(1, true, kPagerModifierMs + 1), 'q');
}

void test_pager_enter_backspace() {
  PagerKeys k;
  expectAction(k.key(20, true, 0), Action::Select);
  expectAction(k.key(30, true, 0), Action::Backspace);
}

void test_pager_sym_shortcuts() {
  PagerKeys k;
  k.key(21, true, 0);
  expectAction(k.key(30, true, 10), Action::OpenHome);
  k.key(21, true, 20);
  expectAction(k.key(20, true, 30), Action::Palette);
}

void test_pager_backspace_repeats_while_held() {
  PagerKeys k;
  expectAction(k.key(30, true, 0), Action::Backspace);
  expectAction(k.poll(kPagerRepeatDelayMs - 1), Action::None);
  expectAction(k.poll(kPagerRepeatDelayMs), Action::Backspace);
  expectAction(k.poll(kPagerRepeatDelayMs + kPagerRepeatMs - 1), Action::None);
  expectAction(k.poll(kPagerRepeatDelayMs + kPagerRepeatMs), Action::Backspace);
  k.key(30, false, kPagerRepeatDelayMs + kPagerRepeatMs + 5);
  expectAction(k.poll(10000), Action::None);
}

void test_pager_home_does_not_repeat() {
  PagerKeys k;
  k.key(21, true, 0);
  expectAction(k.key(30, true, 10), Action::OpenHome);
  expectAction(k.poll(5000), Action::None);
}

// One detent of the quadrature wheel: four transitions of (B<<1)|A.
static void turn(Rotary& r, bool positive) {
  const uint8_t pos[4] = {2, 3, 1, 0};
  const uint8_t neg[4] = {1, 3, 2, 0};
  for (uint8_t ab : positive ? pos : neg) r.sample(ab);
}

void test_rotary_detents() {
  Rotary r;
  r.reset(0);
  TEST_ASSERT_EQUAL(0, r.take());
  turn(r, true);
  TEST_ASSERT_EQUAL(1, r.take());
  TEST_ASSERT_EQUAL(0, r.take());
  turn(r, false);
  TEST_ASSERT_EQUAL(-1, r.take());
}

void test_rotary_partial_detent() {
  Rotary r;
  r.reset(0);
  r.sample(2);
  r.sample(3);
  TEST_ASSERT_EQUAL(0, r.take());
  r.sample(2);
  r.sample(0);
  TEST_ASSERT_EQUAL(0, r.take());
}

void test_rotary_queue_and_cap() {
  Rotary r;
  r.reset(0);
  turn(r, true);
  turn(r, true);
  TEST_ASSERT_EQUAL(1, r.take());
  TEST_ASSERT_EQUAL(1, r.take());
  TEST_ASSERT_EQUAL(0, r.take());
  for (int i = 0; i < kRotaryQueueMax + 5; ++i) turn(r, false);
  int n = 0;
  while (r.take() == -1) ++n;
  TEST_ASSERT_EQUAL(kRotaryQueueMax, n);
}

void test_rotary_reverse_cancels_queue() {
  Rotary r;
  r.reset(0);
  turn(r, true);
  turn(r, true);
  turn(r, false);
  TEST_ASSERT_EQUAL(1, r.take());
  TEST_ASSERT_EQUAL(0, r.take());
}

void test_rotary_bounce_ignored() {
  Rotary r;
  r.reset(0);
  r.sample(0);
  r.sample(3);  // an impossible jump counts for nothing
  r.sample(0);
  TEST_ASSERT_EQUAL(0, r.take());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pager_letters);
  RUN_TEST(test_pager_release_ignored);
  RUN_TEST(test_pager_shift_one_shot);
  RUN_TEST(test_pager_sym);
  RUN_TEST(test_pager_modifier_toggles_off);
  RUN_TEST(test_pager_modifier_times_out);
  RUN_TEST(test_pager_enter_backspace);
  RUN_TEST(test_pager_sym_shortcuts);
  RUN_TEST(test_pager_backspace_repeats_while_held);
  RUN_TEST(test_pager_home_does_not_repeat);
  RUN_TEST(test_rotary_detents);
  RUN_TEST(test_rotary_partial_detent);
  RUN_TEST(test_rotary_queue_and_cap);
  RUN_TEST(test_rotary_reverse_cancels_queue);
  RUN_TEST(test_rotary_bounce_ignored);
  return UNITY_END();
}
