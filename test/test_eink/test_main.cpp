#include <unity.h>

#include <cstdint>

#include "ui/eink.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectDue(EinkRefresh want, EinkRefresh got) {
  TEST_ASSERT_EQUAL(static_cast<int>(want), static_cast<int>(got));
}

void test_dark_threshold() {
  TEST_ASSERT_TRUE(rgb565Dark(0x0000));
  TEST_ASSERT_FALSE(rgb565Dark(0xFFFF));
  TEST_ASSERT_TRUE(rgb565Dark(0x738E));   // 0x707070
  TEST_ASSERT_FALSE(rgb565Dark(0xD69A));  // 0xD0D0D0
  TEST_ASSERT_TRUE(rgb565Dark(0x001F));   // pure blue is dark
  TEST_ASSERT_FALSE(rgb565Dark(0x7FEF));  // light green is light
}

void test_pack_rows_msb_first_white_is_one() {
  // 10 px wide: two bytes a row, the last six bits of each row padding.
  uint16_t frame[10 * 3];
  for (uint16_t& p : frame) p = 0xFFFF;
  frame[10 * 1 + 0] = 0x0000;  // row 1, x 0
  frame[10 * 1 + 9] = 0x0000;  // row 1, x 9
  uint8_t mono[2 * 3] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  packMono(frame, 10, 1, 2, mono);
  // Row 0 and 2 untouched; row 1 black at x 0 and x 9.
  TEST_ASSERT_EQUAL_HEX8(0xAA, mono[0]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, mono[2]);
  TEST_ASSERT_EQUAL_HEX8(0xBF, mono[3]);
  TEST_ASSERT_EQUAL_HEX8(0xAA, mono[4]);
}

void test_first_refresh_is_full() {
  EinkPolicy p;
  expectDue(EinkRefresh::None, p.due(0, 0, false));
  p.changed(1000);
  expectDue(EinkRefresh::Full, p.due(1000, 0, false));
  p.sent(EinkRefresh::Full, 1000);
  TEST_ASSERT_FALSE(p.pending());
}

void test_min_gap_between_refreshes() {
  EinkPolicy p;
  p.changed(0);
  p.sent(EinkRefresh::Full, 1000);
  p.changed(1100);
  expectDue(EinkRefresh::None, p.due(1100, 0, false));
  expectDue(EinkRefresh::Partial, p.due(1000 + kEinkMinGapMs, 0, false));
}

void test_waits_for_typing_to_pause() {
  EinkPolicy p;
  p.changed(0);
  p.sent(EinkRefresh::Full, 0);
  p.changed(5000);
  expectDue(EinkRefresh::None, p.due(5050, 5000, false));
  expectDue(EinkRefresh::Partial, p.due(5000 + kEinkTypingQuietMs, 5000, false));
  // Keys still coming: held no longer than the cap.
  expectDue(EinkRefresh::None, p.due(5000 + kEinkTypingMaxMs - 1, 5000 + kEinkTypingMaxMs - 10, false));
  expectDue(EinkRefresh::Partial, p.due(5000 + kEinkTypingMaxMs, 5000 + kEinkTypingMaxMs - 10, false));
}

void test_full_every_n_partials() {
  EinkPolicy p;
  uint32_t t = 0;
  p.changed(t);
  p.sent(EinkRefresh::Full, t);
  for (int i = 0; i < kEinkFullEvery; ++i) {
    t += 1000;
    p.changed(t);
    expectDue(EinkRefresh::Partial, p.due(t, 0, false));
    p.sent(EinkRefresh::Partial, t);
  }
  t += 1000;
  p.changed(t);
  expectDue(EinkRefresh::Full, p.due(t, 0, false));
}

void test_big_change_goes_full_after_a_few_partials() {
  EinkPolicy p;
  uint32_t t = 0;
  p.changed(t);
  p.sent(EinkRefresh::Full, t);
  t += 1000;
  p.changed(t);
  expectDue(EinkRefresh::Partial, p.due(t, 0, true));
  for (int i = 0; i < kEinkFullOnBigChange; ++i) {
    p.sent(EinkRefresh::Partial, t);
    t += 1000;
    p.changed(t);
  }
  expectDue(EinkRefresh::Partial, p.due(t, 0, false));
  expectDue(EinkRefresh::Full, p.due(t, 0, true));
}

void test_full_request_and_drop() {
  EinkPolicy p;
  p.changed(0);
  p.sent(EinkRefresh::Full, 0);
  p.changed(1000, true);
  expectDue(EinkRefresh::Full, p.due(1000, 990, false));  // a full one doesn't wait for typing
  p.drop();
  TEST_ASSERT_FALSE(p.pending());
  expectDue(EinkRefresh::None, p.due(5000, 0, false));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_dark_threshold);
  RUN_TEST(test_pack_rows_msb_first_white_is_one);
  RUN_TEST(test_first_refresh_is_full);
  RUN_TEST(test_min_gap_between_refreshes);
  RUN_TEST(test_waits_for_typing_to_pause);
  RUN_TEST(test_full_every_n_partials);
  RUN_TEST(test_big_change_goes_full_after_a_few_partials);
  RUN_TEST(test_full_request_and_drop);
  return UNITY_END();
}
