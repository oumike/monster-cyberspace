#include <unity.h>

#include <string>

#include "cell6x12.h"
#include "ui/emoji_placeholder.h"
#include "ui/emoji_wide.h"
#include "ui/text_width.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_emoji_placeholder_is_one_narrow_glyph() {
  TEST_ASSERT_EQUAL(1, cellWidth(emojiPlaceholder()));
  TEST_ASSERT_NOT_NULL(findGlyph(cell6x12, emojiPlaceholder()));
}

void test_wide_emoji_draws_placeholder_then_blank() {
  const char32_t e = kWideEmoji[0];
  TEST_ASSERT_EQUAL_UINT32(emojiPlaceholder(), emojiStandIn(e));
  TEST_ASSERT_EQUAL_UINT32(U' ', emojiStandIn(wideTail(e)));
  TEST_ASSERT_EQUAL_UINT32(U'a', emojiStandIn(U'a'));
  TEST_ASSERT_EQUAL_UINT32(0x2500, emojiStandIn(0x2500));
}

void test_emoji_keeps_two_cells() {
  // Text with an emoji lays out the same whether or not it is drawn.
  const std::u32string s = std::u32string(U"hi ") + kWideEmoji[0] + U" there";
  TEST_ASSERT_EQUAL(int(s.size()) + 1, textWidth(s));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_emoji_placeholder_is_one_narrow_glyph);
  RUN_TEST(test_wide_emoji_draws_placeholder_then_blank);
  RUN_TEST(test_emoji_keeps_two_cells);
  return UNITY_END();
}
