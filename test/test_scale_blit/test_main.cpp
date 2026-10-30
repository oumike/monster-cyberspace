#include <unity.h>

#include <vector>

#include "input/touch_transform.h"
#include "ui/scale_blit.h"

using namespace rc;

void setUp() {}
void tearDown() {}

// A frame of unique values, so every output pixel says where it came from.
static std::vector<uint16_t> frameOf(int w, int h) {
  std::vector<uint16_t> f(static_cast<size_t>(w) * h);
  for (size_t i = 0; i < f.size(); ++i) f[i] = static_cast<uint16_t>(i + 1);
  return f;
}

// Every panel pixel the blit writes shows the frame pixel a touch there
// resolves to, so the picture and touch can't disagree in any rotation.
static void checkMatchesTouch(uint8_t r, int scale) {
  const int fw = (r & 1) ? 5 : 7, fh = (r & 1) ? 7 : 5;  // native portrait 5x7
  const int nativeW = 5, nativeH = 7;
  const std::vector<uint16_t> f = frameOf(fw, fh);
  std::vector<uint16_t> out(static_cast<size_t>(fw) * fh * scale * scale);
  for (int y0 = 0; y0 < fh; y0 += 2) {
    const int y1 = y0 + 2 < fh ? y0 + 2 : fh;
    const BlitRect rc = scaleBlitRows(f.data(), fw, fh, y0, y1, r, scale, out.data());
    TEST_ASSERT_EQUAL(fw * (y1 - y0) * scale * scale, rc.w * rc.h);
    for (int oy = 0; oy < rc.h; ++oy) {
      for (int ox = 0; ox < rc.w; ++ox) {
        int16_t x, y;
        touchToFrame(static_cast<int16_t>((rc.x + ox) / scale), static_cast<int16_t>((rc.y + oy) / scale),
                     nativeW, nativeH, r, x, y);
        TEST_ASSERT_EQUAL_UINT16(f[static_cast<size_t>(y) * fw + x], out[static_cast<size_t>(oy) * rc.w + ox]);
      }
    }
  }
}

void test_landscape() { checkMatchesTouch(0, 1); }
void test_portrait() { checkMatchesTouch(1, 1); }
void test_landscape_180() { checkMatchesTouch(2, 1); }
void test_portrait_180() { checkMatchesTouch(3, 1); }
void test_doubled_every_rotation() {
  for (uint8_t r = 0; r < 4; ++r) checkMatchesTouch(r, 2);
}

void test_rect_on_the_panel() {
  const std::vector<uint16_t> f = frameOf(7, 5);
  std::vector<uint16_t> out(7 * 2 * 4);
  const BlitRect rc = scaleBlitRows(f.data(), 7, 5, 1, 3, 0, 2, out.data());
  // Landscape: frame rows 1..2 are panel columns 1..2, the full height.
  TEST_ASSERT_EQUAL(2, rc.x);
  TEST_ASSERT_EQUAL(0, rc.y);
  TEST_ASSERT_EQUAL(4, rc.w);
  TEST_ASSERT_EQUAL(14, rc.h);
}

void test_empty_band() {
  const std::vector<uint16_t> f = frameOf(7, 5);
  uint16_t out[1] = {0xAAAA};
  const BlitRect rc = scaleBlitRows(f.data(), 7, 5, 3, 3, 1, 2, out);
  TEST_ASSERT_EQUAL(0, rc.w * rc.h);
  TEST_ASSERT_EQUAL_HEX16(0xAAAA, out[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_landscape);
  RUN_TEST(test_portrait);
  RUN_TEST(test_landscape_180);
  RUN_TEST(test_portrait_180);
  RUN_TEST(test_doubled_every_rotation);
  RUN_TEST(test_rect_on_the_panel);
  RUN_TEST(test_empty_band);
  return UNITY_END();
}
