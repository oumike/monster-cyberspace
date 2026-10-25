#include <unity.h>

#include <initializer_list>

#include "cell6x12.h"
#include "cell8x16.h"
#include "ui/glyph_set.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static bool inked(const CellFont& f, const uint8_t* g) {
  for (int i = 0; i < f.h; ++i)
    if (g[i]) return true;
  return false;
}

void test_dims() {
  TEST_ASSERT_EQUAL(6, cell6x12.w);
  TEST_ASSERT_EQUAL(12, cell6x12.h);
  TEST_ASSERT_EQUAL(8, cell8x16.w);
  TEST_ASSERT_EQUAL(16, cell8x16.h);
  for (const CellFont* f : {&cell6x12, &cell8x16})
    for (int i = 1; i < f->count; ++i) TEST_ASSERT_TRUE(f->codepoints[i - 1] < f->codepoints[i]);
}

void test_ascii_present() {
  for (const CellFont* f : {&cell6x12, &cell8x16}) {
    for (char32_t c = 0x20; c <= 0x7E; ++c) TEST_ASSERT_NOT_NULL(findGlyph(*f, c));
    TEST_ASSERT_FALSE(inked(*f, findGlyph(*f, U' ')));
    TEST_ASSERT_TRUE(inked(*f, findGlyph(*f, U'A')));
    // Nothing is drawn past the cell width.
    uint8_t mask = static_cast<uint8_t>(0xFF >> f->w);
    for (int i = 0; i < f->count * f->h; ++i) TEST_ASSERT_EQUAL(0, f->bitmaps[i] & mask);
  }
}

void test_box_drawing_present() {
  for (const CellFont* f : {&cell6x12, &cell8x16}) {
    for (char32_t c : kExtraGlyphs) TEST_ASSERT_NOT_NULL(findGlyph(*f, c));
    // ─ spans the full cell width on one row.
    const uint8_t* h = findGlyph(*f, 0x2500);
    uint8_t full = static_cast<uint8_t>(0xFF << (8 - f->w));
    bool found = false;
    for (int i = 0; i < f->h; ++i) found |= h[i] == full;
    TEST_ASSERT_TRUE(found);
    // Aliased emoji use their stand-in's bitmap.
    TEST_ASSERT_EQUAL_MEMORY(findGlyph(*f, U'8'), findGlyph(*f, 0x1F3B1), f->h);
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
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dims);
  RUN_TEST(test_ascii_present);
  RUN_TEST(test_box_drawing_present);
  RUN_TEST(test_missing_falls_back);
  return UNITY_END();
}
