#include <unity.h>

#include <string>

#include "feed/markdown_text.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_md_headings_emphasis() {
  TEST_ASSERT_TRUE(U"Title\nbold and it" == markdownToText("## Title\n**bold** and *it*"));
  TEST_ASSERT_TRUE(U"2 * 3" == markdownToText("2 * 3"));
}

void test_md_links_images() {
  TEST_ASSERT_TRUE(U"see site <https://a.b>" == markdownToText("see [site](https://a.b)"));
  TEST_ASSERT_TRUE(U"[img: cat] [img]" == markdownToText("![cat](https://x/c.png) ![](https://x/d.png)"));
}

void test_md_code_kept_verbatim() {
  TEST_ASSERT_TRUE(U"a **b**\nx_y" == markdownToText("```\na **b**\n```\n`x_y`"));
}

void test_md_lists_quotes_blank_lines() {
  TEST_ASSERT_TRUE(U"• one\n• two\n│ quoted\n\nend" ==
                   markdownToText("- one\n* two\n> quoted\n\n\n\nend"));
}

void test_md_utf8_and_empty() {
  TEST_ASSERT_TRUE(U"héllo 🦉" == markdownToText("_héllo_ 🦉"));
  TEST_ASSERT_TRUE(U"" == markdownToText(""));
}

void test_truncate_utf8_boundary() {
  std::string s = "ab🦉";  // 2 + 4 bytes
  TEST_ASSERT_TRUE(truncateUtf8(s, 4));
  TEST_ASSERT_EQUAL_STRING("ab", s.c_str());
  std::string t = "ok";
  TEST_ASSERT_FALSE(truncateUtf8(t, 16));
  std::string u = "日本語";
  TEST_ASSERT_TRUE(truncateCodePoints(u, 2));
  TEST_ASSERT_EQUAL_STRING("日本", u.c_str());
}

void test_next_smaller_limit() {
  TEST_ASSERT_EQUAL(5, nextSmallerLimit(10));
  TEST_ASSERT_EQUAL(2, nextSmallerLimit(5));
  TEST_ASSERT_EQUAL(0, nextSmallerLimit(2));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_md_headings_emphasis);
  RUN_TEST(test_md_links_images);
  RUN_TEST(test_md_code_kept_verbatim);
  RUN_TEST(test_md_lists_quotes_blank_lines);
  RUN_TEST(test_md_utf8_and_empty);
  RUN_TEST(test_truncate_utf8_boundary);
  RUN_TEST(test_next_smaller_limit);
  return UNITY_END();
}
