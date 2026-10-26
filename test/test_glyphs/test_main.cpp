#include <unity.h>

#include <initializer_list>

#include "cell10x18.h"
#include "cell6x12.h"
#include "cell8x14.h"
#include "cell8x16.h"
#include "ui/glyph_set.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static const std::initializer_list<const CellFont*> kFonts = {&cell6x12, &cell8x14, &cell8x16,
                                                               &cell10x18};

static bool inked(const CellFont& f, const uint8_t* g) {
  for (int i = 0; i < f.h * rowBytes(f); ++i)
    if (g[i]) return true;
  return false;
}

void test_dims() {
  TEST_ASSERT_EQUAL(6, cell6x12.w);
  TEST_ASSERT_EQUAL(12, cell6x12.h);
  TEST_ASSERT_EQUAL(8, cell8x16.w);
  TEST_ASSERT_EQUAL(16, cell8x16.h);
  TEST_ASSERT_EQUAL(8, cell8x14.w);
  TEST_ASSERT_EQUAL(14, cell8x14.h);
  TEST_ASSERT_EQUAL(10, cell10x18.w);
  TEST_ASSERT_EQUAL(18, cell10x18.h);
  TEST_ASSERT_EQUAL(1, rowBytes(cell8x16));
  TEST_ASSERT_EQUAL(2, rowBytes(cell10x18));
  for (const CellFont* f : kFonts)
    for (int i = 1; i < f->count; ++i) TEST_ASSERT_TRUE(f->codepoints[i - 1] < f->codepoints[i]);
}

void test_ascii_present() {
  for (const CellFont* f : kFonts) {
    for (char32_t c = 0x20; c <= 0x7E; ++c) TEST_ASSERT_NOT_NULL(findGlyph(*f, c));
    TEST_ASSERT_FALSE(inked(*f, findGlyph(*f, U' ')));
    TEST_ASSERT_TRUE(inked(*f, findGlyph(*f, U'A')));
    // Nothing is drawn past the cell width.
    const int rb = rowBytes(*f);
    for (int g = 0; g < f->count; ++g)
      for (int y = 0; y < f->h; ++y)
        for (int x = f->w; x < rb * 8; ++x)
          TEST_ASSERT_FALSE(glyphPixel(*f, f->bitmaps + size_t(g) * f->h * rb, x, y));
  }
}

void test_box_drawing_present() {
  for (const CellFont* f : kFonts) {
    for (char32_t c : kExtraGlyphs) TEST_ASSERT_NOT_NULL(findGlyph(*f, c));
    // ─ spans the full cell width on one row.
    const uint8_t* h = findGlyph(*f, 0x2500);
    bool found = false;
    for (int y = 0; y < f->h; ++y) {
      bool full = true;
      for (int x = 0; x < f->w; ++x) full &= glyphPixel(*f, h, x, y);
      found |= full;
    }
    TEST_ASSERT_TRUE(found);
    // Aliased emoji use their stand-in's bitmap.
    TEST_ASSERT_EQUAL_MEMORY(findGlyph(*f, U'8'), findGlyph(*f, 0x1F3B1), f->h * rowBytes(*f));
  }
}

void test_feed_marks_use_runtime_aliases() {
  for (const CellFont* f : kFonts) {
    TEST_ASSERT_EQUAL_PTR(findGlyph(*f, 0x2190), glyphFor(*f, 0x21A9));  // ↩ → ←
    TEST_ASSERT_EQUAL_PTR(findGlyph(*f, U'*'), glyphFor(*f, 0x2605));   // ★ → *
    TEST_ASSERT_EQUAL_PTR(findGlyph(*f, 0x2514), glyphFor(*f, 0x21B3));  // ↳ → └
  }
}

void test_missing_falls_back() {
  const uint8_t* g = glyphFor(cell8x16, 0x4E2D);  // 中
  TEST_ASSERT_NOT_NULL(g);
  TEST_ASSERT_EQUAL_PTR(findGlyph(cell8x16, 0xFFFD), g);
  TEST_ASSERT_EQUAL_PTR(findGlyph(cell6x12, U'x'), glyphFor(cell6x12, U'x'));

  static const uint32_t cps[] = {U'?', U'A'};
  static const uint8_t bm[] = {1, 2};
  CellFont tiny{8, 1, 2, cps, bm};
  TEST_ASSERT_EQUAL_PTR(&bm[0], glyphFor(tiny, 0x2500));

  // Wide cells take two bytes a row, so the second glyph starts 2*h bytes in.
  static const uint8_t wide[] = {0x00, 0x00, 0x80, 0x40};
  CellFont two{10, 1, 2, cps, wide};
  TEST_ASSERT_EQUAL_PTR(&wide[2], findGlyph(two, U'A'));
  TEST_ASSERT_TRUE(glyphPixel(two, &wide[2], 0, 0));
  TEST_ASSERT_TRUE(glyphPixel(two, &wide[2], 9, 0));
  TEST_ASSERT_FALSE(glyphPixel(two, &wide[2], 8, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dims);
  RUN_TEST(test_ascii_present);
  RUN_TEST(test_box_drawing_present);
  RUN_TEST(test_feed_marks_use_runtime_aliases);
  RUN_TEST(test_missing_falls_back);
  return UNITY_END();
}
