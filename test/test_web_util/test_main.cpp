#include <unity.h>

#include <string>

#include "web/web_util.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_black_and_white() {
  TEST_ASSERT_EQUAL_STRING("#000000", rgb565Hex(0x0000).c_str());
  TEST_ASSERT_EQUAL_STRING("#ffffff", rgb565Hex(0xFFFF).c_str());
}

void test_channels_widen_by_repeating_their_top_bits() {
  TEST_ASSERT_EQUAL_STRING("#ff0000", rgb565Hex(0xF800).c_str());
  TEST_ASSERT_EQUAL_STRING("#00ff00", rgb565Hex(0x07E0).c_str());
  TEST_ASSERT_EQUAL_STRING("#0000ff", rgb565Hex(0x001F).c_str());
  // 0x7CFC7C packed: r 15, g 63, b 15.
  TEST_ASSERT_EQUAL_STRING("#7bff7b", rgb565Hex(uint16_t(15 << 11 | 63 << 5 | 15)).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_black_and_white);
  RUN_TEST(test_channels_widen_by_repeating_their_top_bits);
  return UNITY_END();
}
