// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include "ui/tilt_rotate.h"

using rc::TiltRotator;

void setUp() {}
void tearDown() {}

// Native axes: x right, y down. Portrait (1) is the native frame, so gravity
// down the native y is portrait; landscape (0) has its down at native -x.
void test_pick_each_rotation() {
  TEST_ASSERT_EQUAL(1, TiltRotator::pick(0, 1, 0));
  TEST_ASSERT_EQUAL(3, TiltRotator::pick(0, -1, 0));
  TEST_ASSERT_EQUAL(0, TiltRotator::pick(-1, 0, 0));
  TEST_ASSERT_EQUAL(2, TiltRotator::pick(1, 0, 0));
}

void test_pick_flat_and_diagonal() {
  TEST_ASSERT_EQUAL(-1, TiltRotator::pick(0.1f, 0.1f, 1));  // lying on a table
  TEST_ASSERT_EQUAL(-1, TiltRotator::pick(1, 1, 0));        // 45°: the dead zone
  TEST_ASSERT_EQUAL(1, TiltRotator::pick(0.5f, 1, 0.3f));   // ~27° off portrait
  TEST_ASSERT_EQUAL(-1, TiltRotator::pick(0, 0, 0));
}

void test_update_settles_after_delay() {
  TiltRotator t;
  TEST_ASSERT_EQUAL(-1, t.update(0, 1, 0, 1000));
  TEST_ASSERT_EQUAL(-1, t.update(0, 1, 0, 1400));
  TEST_ASSERT_EQUAL(1, t.update(0, 1, 0, 1500));
  // A brief tilt to landscape does not stick; a held one does.
  TEST_ASSERT_EQUAL(1, t.update(-1, 0, 0, 2000));
  TEST_ASSERT_EQUAL(1, t.update(0, 1, 0, 2300));
  TEST_ASSERT_EQUAL(1, t.update(-1, 0, 0, 2400));
  TEST_ASSERT_EQUAL(1, t.update(-1, 0, 0, 2800));
  TEST_ASSERT_EQUAL(0, t.update(-1, 0, 0, 2900));
}

void test_update_keeps_rotation_when_flat() {
  TiltRotator t;
  t.update(1, 0, 0, 0);
  TEST_ASSERT_EQUAL(2, t.update(1, 0, 0, 600));
  TEST_ASSERT_EQUAL(2, t.update(0, 0, 1, 5000));
  TEST_ASSERT_EQUAL(2, t.update(1, 1, 0, 9000));
  t.reset();
  TEST_ASSERT_EQUAL(-1, t.settled());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pick_each_rotation);
  RUN_TEST(test_pick_flat_and_diagonal);
  RUN_TEST(test_update_settles_after_delay);
  RUN_TEST(test_update_keeps_rotation_when_flat);
  return UNITY_END();
}
