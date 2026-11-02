#include <unity.h>

#include "ui/scale2x.h"

using namespace rc::scale2x;

void setUp() {}
void tearDown() {}

static void expectPt(Pt p, int x, int y) {
  TEST_ASSERT_EQUAL(x, p.x);
  TEST_ASSERT_EQUAL(y, p.y);
}

void test_frame_size() {
  int w, h;
  frameSize(284, 616, 1, w, h);
  TEST_ASSERT_EQUAL(284, w);
  TEST_ASSERT_EQUAL(616, h);
  frameSize(284, 616, 0, w, h);
  TEST_ASSERT_EQUAL(616, w);
  TEST_ASSERT_EQUAL(284, h);
}

void test_portrait_identity() {
  expectPt(frameToNative(0, 0, 284, 616, 1), 0, 0);
  expectPt(frameToNative(283, 615, 284, 616, 1), 283, 615);
}

void test_portrait_180() { expectPt(frameToNative(0, 0, 284, 616, 3), 283, 615); }

void test_landscape() {
  expectPt(frameToNative(0, 0, 284, 616, 0), 283, 0);
  expectPt(frameToNative(615, 283, 284, 616, 0), 0, 615);
  expectPt(frameToNative(615, 0, 284, 616, 0), 283, 615);
}

void test_landscape_180() {
  expectPt(frameToNative(0, 0, 284, 616, 2), 0, 615);
  expectPt(frameToNative(615, 283, 284, 616, 2), 283, 0);
}

void test_round_trip() {
  for (uint8_t r = 0; r < 4; r++) {
    int w, h;
    frameSize(284, 616, r, w, h);
    const Pt pts[] = {{0, 0}, {w - 1, h - 1}, {w - 1, 0}, {0, h - 1}, {37, 101}};
    for (const Pt& p : pts) {
      Pt n = frameToNative(p.x, p.y, 284, 616, r);
      TEST_ASSERT_TRUE(n.x >= 0 && n.x < 284 && n.y >= 0 && n.y < 616);
      expectPt(nativeToFrame(n.x, n.y, 284, 616, r), p.x, p.y);
    }
  }
}

void test_native_rows() {
  int a, b;
  nativeRows(10, 20, 284, 616, 1, a, b);
  TEST_ASSERT_EQUAL(10, a);
  TEST_ASSERT_EQUAL(20, b);
  nativeRows(0, 1, 284, 616, 3, a, b);
  TEST_ASSERT_EQUAL(615, a);
  TEST_ASSERT_EQUAL(616, b);
  nativeRows(5, 6, 284, 616, 0, a, b);
  TEST_ASSERT_EQUAL(0, a);
  TEST_ASSERT_EQUAL(616, b);
}

void test_raw_to_native() {
  expectPt(rawToNative(0, 0, 1060, 2400, 284, 616), 0, 0);
  expectPt(rawToNative(1059, 2399, 1060, 2400, 284, 616), 283, 615);
  expectPt(rawToNative(530, 1200, 1060, 2400, 284, 616), 142, 308);
  expectPt(rawToNative(-5, 3000, 1060, 2400, 284, 616), 0, 615);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_frame_size);
  RUN_TEST(test_portrait_identity);
  RUN_TEST(test_portrait_180);
  RUN_TEST(test_landscape);
  RUN_TEST(test_landscape_180);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_native_rows);
  RUN_TEST(test_raw_to_native);
  return UNITY_END();
}
