#include <unity.h>

#include "ui/screen_timeout.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_timeout_options_cycle_and_labels() {
  TEST_ASSERT_EQUAL_UINT32(60, nextScreenTimeout(30));
  TEST_ASSERT_EQUAL_UINT32(0, nextScreenTimeout(3600));  // then Never
  TEST_ASSERT_EQUAL_UINT32(15, nextScreenTimeout(0));    // and round again
  TEST_ASSERT_EQUAL_UINT32(15, nextScreenTimeout(45));   // off the list: the first
  TEST_ASSERT_EQUAL_STRING("15 s", screenTimeoutLabel(15).c_str());
  TEST_ASSERT_EQUAL_STRING("2 min", screenTimeoutLabel(120).c_str());
  TEST_ASSERT_EQUAL_STRING("Never", screenTimeoutLabel(0).c_str());
  TEST_ASSERT_TRUE(validScreenTimeout(1800));
  TEST_ASSERT_TRUE(validScreenTimeout(0));
  TEST_ASSERT_FALSE(validScreenTimeout(45));
}

void test_idle_phases() {
  // 30 s: full until 20 s, dimmed for the last 10 s, then locked.
  TEST_ASSERT_EQUAL(int(IdlePhase::Active), int(idlePhase(19999, 30)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Dim), int(idlePhase(20000, 30)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Lock), int(idlePhase(30000, 30)));
  // 15 s dims after 5 s; Never never moves.
  TEST_ASSERT_EQUAL(int(IdlePhase::Dim), int(idlePhase(5000, 15)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Active), int(idlePhase(int64_t(1) << 40, 0)));
}

void test_bouncer_stays_inside_and_reflects() {
  Bouncer b{0, 2, 1, -1};
  b.step(3, 2);  // a 4 x 3 box of positions
  TEST_ASSERT_EQUAL(1, b.x);
  TEST_ASSERT_EQUAL(1, b.y);
  for (int i = 0; i < 50; ++i) {
    b.step(3, 2);
    TEST_ASSERT_TRUE(b.x >= 0 && b.x <= 3);
    TEST_ASSERT_TRUE(b.y >= 0 && b.y <= 2);
  }
  Bouncer still{0, 0, 1, 1};
  still.step(0, 0);  // no room to move
  TEST_ASSERT_EQUAL(0, still.x);
  TEST_ASSERT_EQUAL(0, still.y);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_timeout_options_cycle_and_labels);
  RUN_TEST(test_idle_phases);
  RUN_TEST(test_bouncer_stays_inside_and_reflects);
  return UNITY_END();
}
