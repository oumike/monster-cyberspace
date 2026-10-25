#include <unity.h>

#include "util/base64.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_utf8_roundtrip() {
  std::string s = "h\xc3\xa9llo \xf0\x9f\x8e\xb2";  // "héllo 🎲"
  std::u32string u = utf8ToU32(s);
  TEST_ASSERT_EQUAL(7, u.size());
  TEST_ASSERT_EQUAL_HEX32(0xE9, u[1]);
  TEST_ASSERT_EQUAL_HEX32(0x1F3B2, u[6]);
  TEST_ASSERT_EQUAL(7, utf8Length(s));
  TEST_ASSERT_TRUE(u32ToUtf8(u) == s);
}

void test_utf8_invalid() {
  std::u32string u = utf8ToU32("a\xff" "b");
  TEST_ASSERT_EQUAL(3, u.size());
  TEST_ASSERT_EQUAL_HEX32('a', u[0]);
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, u[1]);
  TEST_ASSERT_EQUAL_HEX32('b', u[2]);
}

void test_utf8_truncated() {
  std::u32string u = utf8ToU32("\xe2\x94");
  TEST_ASSERT_TRUE(u.size() >= 1 && u.size() <= 2);
  for (char32_t c : u) TEST_ASSERT_EQUAL_HEX32(0xFFFD, c);
}

void test_base64_art() {
  std::string out;
  TEST_ASSERT_TRUE(base64Decode("IC9cXy9cCiggby5vICkKID4gXiA8", out));
  TEST_ASSERT_EQUAL_STRING(" /\\_/\\\n( o.o )\n > ^ <", out.c_str());
}

void test_base64_invalid() {
  std::string out;
  TEST_ASSERT_FALSE(base64Decode("@@@", out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_utf8_roundtrip);
  RUN_TEST(test_utf8_invalid);
  RUN_TEST(test_utf8_truncated);
  RUN_TEST(test_base64_art);
  RUN_TEST(test_base64_invalid);
  return UNITY_END();
}
