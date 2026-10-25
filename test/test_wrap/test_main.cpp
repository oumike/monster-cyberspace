#include <unity.h>

#include <string>
#include <vector>

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
              wrapText(dice, 2, 2));
}

void test_empty() {
  assertLines({""}, wrapText(U"", 20, 20));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fits);
  RUN_TEST(test_wraps_at_space);
  RUN_TEST(test_hard_split);
  RUN_TEST(test_newline);
  RUN_TEST(test_multibyte_unsplit);
  RUN_TEST(test_empty);
  return UNITY_END();
}
