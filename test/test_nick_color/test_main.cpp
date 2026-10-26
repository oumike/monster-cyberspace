#include <unity.h>

#include "ui/nick_color.h"
#include "ui/theme.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_fnv_values() {
  TEST_ASSERT_EQUAL_UINT32(1666852401u, fnv1a32Lower("tardibyte"));
  TEST_ASSERT_EQUAL_UINT32(2518148919u, fnv1a32Lower("genghis_khan"));
  TEST_ASSERT_EQUAL_UINT32(727871219u, fnv1a32Lower("oumike"));
  TEST_ASSERT_EQUAL_UINT32(3826002220u, fnv1a32Lower("A"));
  TEST_ASSERT_EQUAL_UINT32(2166136261u, fnv1a32Lower(""));
}

void test_hue() {
  TEST_ASSERT_EQUAL_UINT16(201, nickHue("tardibyte"));
  TEST_ASSERT_EQUAL_UINT16(39, nickHue("genghis_khan"));
  TEST_ASSERT_EQUAL_UINT16(179, nickHue("oumike"));
  TEST_ASSERT_EQUAL_UINT16(340, nickHue("A"));
}

void test_hsl() {
  TEST_ASSERT_EQUAL_HEX16(0xF800, hslToRgb565(0, 1, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, hslToRgb565(120, 1, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(0x001F, hslToRgb565(240, 1, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, hslToRgb565(0, 0, 1));
}

void test_self_gold() { TEST_ASSERT_EQUAL_HEX16(0xFE8C, nickColor(defaultTheme(), "x", true)); }

void test_theme_bg_black() { TEST_ASSERT_EQUAL_HEX16(0x0000, defaultTheme().bg); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fnv_values);
  RUN_TEST(test_hue);
  RUN_TEST(test_hsl);
  RUN_TEST(test_self_gold);
  RUN_TEST(test_theme_bg_black);
  return UNITY_END();
}
