#include <unity.h>

#include <string>
#include <vector>

#include "ui/emoji_set.h"
#include "ui/text_width.h"
#include "ui/wrap.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void assertLines(const std::vector<std::string>& expected,
                        const std::vector<std::u32string>& actual) {
  TEST_ASSERT_EQUAL(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), u32ToUtf8(actual[i]).c_str());
  }
}

void test_fits() {
  assertLines({"hi there"}, wrapText(U"hi there", 20, 20));
}

void test_wraps_at_space() {
  assertLines({"anyone else on the", "4.0 beta yet?"},
              wrapText(U"anyone else on the 4.0 beta yet?", 20, 14));
}

void test_hard_split() {
  std::u32string text(2048, U'a');
  auto lines = wrapText(text, 47, 41);
  TEST_ASSERT_TRUE(lines.size() > 1);
  TEST_ASSERT_EQUAL(47, lines[0].size());
  size_t total = lines[0].size();
  for (size_t i = 1; i < lines.size(); ++i) {
    TEST_ASSERT_TRUE(lines[i].size() <= 41);
    total += lines[i].size();
  }
  TEST_ASSERT_EQUAL(2048, total);
}

void test_newline() {
  assertLines({"a", "b"}, wrapText(U"a\nb", 20, 20));
}

void test_multibyte_unsplit() {
  std::u32string dice = utf8ToU32("\xf0\x9f\x8e\xb2\xf0\x9f\x8e\xb2\xf0\x9f\x8e\xb2");
  assertLines({"\xf0\x9f\x8e\xb2\xf0\x9f\x8e\xb2", "\xf0\x9f\x8e\xb2"},
              wrapText(dice, 4, 4));  // each die is two cells
}

void test_empty() {
  assertLines({""}, wrapText(U"", 20, 20));
}

void test_emoji_counts_two_cells() {
  std::u32string s = U"ab";
  s += kTrayEmoji[0];
  s += U"cd";
  TEST_ASSERT_EQUAL(6, textWidth(s));
  TEST_ASSERT_EQUAL(2, int(fitChars(s, 3)));
  TEST_ASSERT_EQUAL(3, int(fitChars(s, 4)));
  TEST_ASSERT_EQUAL(0, cellWidth(0xFE0F));
  TEST_ASSERT_EQUAL(2, cellWidth(0x1F680));  // 🚀: any emoji, not just the tray's
  TEST_ASSERT_EQUAL(2, cellWidth(0x1F3B2));  // 🎲
  TEST_ASSERT_EQUAL(0, cellWidth(0x1F3FD));  // skin tone modifier
  TEST_ASSERT_EQUAL(0, cellWidth(0xE0067));  // tag
  TEST_ASSERT_EQUAL(0, cellWidth(0x20E3));   // keycap
  TEST_ASSERT_EQUAL(1, cellWidth(0x263A));   // ☺: Terminus draws it, one cell
  TEST_ASSERT_EQUAL(1, cellWidth(0x21A9));   // ↩: a UI mark, kept narrow
  TEST_ASSERT_EQUAL(1, cellWidth(0x03B1));   // α
  const std::u32string a = U"ab", b = std::u32string(1, kTrayEmoji[0]) + U"c";
  auto lines = wrapText(s, 3, 3);
  TEST_ASSERT_EQUAL(3, int(lines.size()));
  TEST_ASSERT_TRUE(lines[0] == a);
  TEST_ASSERT_TRUE(lines[1] == b);
  TEST_ASSERT_TRUE(lines[2] == U"d");
  // A wide emoji in a one-cell width still moves on.
  TEST_ASSERT_EQUAL(1, int(wrapText(std::u32string(1, kTrayEmoji[0]), 1, 1).size()));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fits);
  RUN_TEST(test_wraps_at_space);
  RUN_TEST(test_hard_split);
  RUN_TEST(test_newline);
  RUN_TEST(test_multibyte_unsplit);
  RUN_TEST(test_empty);
  RUN_TEST(test_emoji_counts_two_cells);
  return UNITY_END();
}
