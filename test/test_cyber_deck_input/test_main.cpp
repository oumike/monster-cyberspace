#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_cyber_deck.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {

struct Out {
  KeyEvent ev[8];
  int n = 0;
};

// One read of both halves (pressed bits set).
Out feed(CyberDeckGamepad& g, uint8_t left, uint8_t right, uint32_t ms) {
  Out o;
  o.n = g.update(left, right, ms, o.ev, 8);
  return o;
}

void expectNone(const Out& o) { TEST_ASSERT_EQUAL(0, o.n); }

void expectOne(const Out& o, Action a) {
  TEST_ASSERT_EQUAL(1, o.n);
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(o.ev[0].action));
}

// Press at ms, then two released reads (a release must read twice).
Out tapRight(CyberDeckGamepad& g, uint8_t bit, uint32_t ms) {
  Out press = feed(g, 0, bit, ms);
  TEST_ASSERT_EQUAL(0, press.n);
  expectNone(feed(g, 0, 0, ms + 10));
  return feed(g, 0, 0, ms + 20);
}

}  // namespace

void test_dpad_arrows_on_press() {
  CyberDeckGamepad g;
  expectOne(feed(g, kCyberDeckUp, 0, 0), Action::Up);
  feed(g, 0, 0, 10);
  feed(g, 0, 0, 20);
  expectOne(feed(g, kCyberDeckDown, 0, 30), Action::Down);
  feed(g, 0, 0, 40);
  feed(g, 0, 0, 50);
  expectOne(feed(g, kCyberDeckLeft, 0, 60), Action::Left);
  feed(g, 0, 0, 70);
  feed(g, 0, 0, 80);
  expectOne(feed(g, kCyberDeckRight, 0, 90), Action::Right);
}

// A shares Right's bit (1 << 3) on the other half: it must not steer.
void test_a_is_select_not_right() {
  static_assert(kCyberDeckA == kCyberDeckRight, "the test relies on the shared bit");
  CyberDeckGamepad g;
  expectOne(tapRight(g, kCyberDeckA, 0), Action::Select);
}

void test_b_and_l1_are_back() {
  CyberDeckGamepad g;
  expectOne(tapRight(g, kCyberDeckB, 0), Action::Back);
  expectNone(feed(g, kCyberDeckL1, 0, 100));
  expectNone(feed(g, 0, 0, 110));
  expectOne(feed(g, 0, 0, 120), Action::Back);
}

// B on the left half's bit is the D-pad's Left, not Back.
void test_halves_are_distinct() {
  static_assert(kCyberDeckB == kCyberDeckLeft, "the test relies on the shared bit");
  CyberDeckGamepad g;
  expectOne(feed(g, kCyberDeckLeft, 0, 0), Action::Left);
  expectNone(feed(g, 0, 0, 10));
  expectNone(feed(g, 0, 0, 20));
}

void test_release_reads_twice() {
  CyberDeckGamepad g;
  expectNone(feed(g, 0, kCyberDeckA, 0));
  expectNone(feed(g, 0, 0, 10));  // one released read: maybe bounce
  expectNone(feed(g, 0, kCyberDeckA, 20));  // pressed again: it was bounce
  expectNone(feed(g, 0, 0, 30));
  expectOne(feed(g, 0, 0, 40), Action::Select);
  expectNone(feed(g, 0, 0, 50));  // and only once
}

void test_hold_replaces_tap() {
  CyberDeckGamepad g;
  expectNone(feed(g, 0, kCyberDeckA, 0));
  expectNone(feed(g, 0, kCyberDeckA, kCyberDeckHoldMs - 1));
  expectOne(feed(g, 0, kCyberDeckA, kCyberDeckHoldMs), Action::ScreenOff);
  expectNone(feed(g, 0, kCyberDeckA, kCyberDeckHoldMs + 500));  // fires once
  expectNone(feed(g, 0, 0, kCyberDeckHoldMs + 510));
  expectNone(feed(g, 0, 0, kCyberDeckHoldMs + 520));  // no Select after the hold

  expectNone(feed(g, 0, kCyberDeckB, 5000));
  expectOne(feed(g, 0, kCyberDeckB, 5000 + kCyberDeckHoldMs), Action::OpenHome);
  feed(g, 0, 0, 7000);
  expectNone(feed(g, 0, 0, 7010));

  expectNone(feed(g, kCyberDeckL1, 0, 9000));
  expectOne(feed(g, kCyberDeckL1, 0, 9000 + kCyberDeckHoldMs), Action::OpenHome);
}

void test_arrow_repeats_while_held() {
  CyberDeckGamepad g;
  expectOne(feed(g, kCyberDeckDown, 0, 0), Action::Down);
  expectNone(feed(g, kCyberDeckDown, 0, kCyberDeckRepeatDelayMs - 1));
  expectOne(feed(g, kCyberDeckDown, 0, kCyberDeckRepeatDelayMs), Action::Down);
  expectNone(feed(g, kCyberDeckDown, 0, kCyberDeckRepeatDelayMs + kCyberDeckRepeatMs - 1));
  expectOne(feed(g, kCyberDeckDown, 0, kCyberDeckRepeatDelayMs + kCyberDeckRepeatMs), Action::Down);
  // Released: no more repeats, and no tap action for an arrow.
  expectNone(feed(g, 0, 0, 2000));
  expectNone(feed(g, 0, 0, 2010));
  expectNone(feed(g, 0, 0, 3000));
}

// readGamepad reports a half that failed its read as 0: one such read is
// absorbed like bounce, so a held button doesn't fire early.
void test_one_failed_half_read_is_absorbed() {
  CyberDeckGamepad g;
  expectNone(feed(g, 0, kCyberDeckA, 0));
  // The right half failed (0) while the left half read Up: only Up fires.
  expectOne(feed(g, kCyberDeckUp, 0, 10), Action::Up);
  expectNone(feed(g, kCyberDeckUp, kCyberDeckA, 20));  // A still held, no Select
  expectNone(feed(g, 0, 0, 30));
  Out o = feed(g, 0, 0, 40);  // A's release, read twice: one Select
  TEST_ASSERT_EQUAL(1, o.n);
  TEST_ASSERT_EQUAL(static_cast<int>(Action::Select), static_cast<int>(o.ev[0].action));
}

void test_unmapped_buttons_do_nothing() {
  CyberDeckGamepad g;
  const uint8_t leftUnmapped = kCyberDeckL2 | kCyberDeckFnL1 | kCyberDeckFnL2;
  const uint8_t rightUnmapped =
      kCyberDeckX | kCyberDeckY | kCyberDeckR1 | kCyberDeckR2 | kCyberDeckFnR1 | kCyberDeckFnR2;
  expectNone(feed(g, leftUnmapped, rightUnmapped, 0));
  expectNone(feed(g, leftUnmapped, rightUnmapped, 5000));  // held: no hold action
  expectNone(feed(g, 0, 0, 5010));
  expectNone(feed(g, 0, 0, 5020));
}

void test_respects_max() {
  CyberDeckGamepad g;
  KeyEvent ev[1];
  TEST_ASSERT_EQUAL(1, g.update(kCyberDeckUp | kCyberDeckLeft, 0, 0, ev, 1));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_dpad_arrows_on_press);
  RUN_TEST(test_a_is_select_not_right);
  RUN_TEST(test_b_and_l1_are_back);
  RUN_TEST(test_halves_are_distinct);
  RUN_TEST(test_release_reads_twice);
  RUN_TEST(test_hold_replaces_tap);
  RUN_TEST(test_arrow_repeats_while_held);
  RUN_TEST(test_one_failed_half_read_is_absorbed);
  RUN_TEST(test_unmapped_buttons_do_nothing);
  RUN_TEST(test_respects_max);
  return UNITY_END();
}
