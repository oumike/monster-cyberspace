#include <unity.h>

#include <string>

#include "input/m9_key_guard.h"

using rc::M9KeyGuard;

void setUp() {}
void tearDown() {}

// Types `text` one key every `paceMs`, each key's contact chattering on the
// way up `chatterMs` after the press (0: no chatter), and returns what the
// guard lets through.
static std::string typeWithChatter(const char* text, uint32_t paceMs, uint32_t chatterMs) {
  M9KeyGuard g;
  std::string out;
  uint32_t t = 1000;
  for (const char* p = text; *p; ++p, t += paceMs) {
    if (g.accept(static_cast<uint8_t>(*p), t)) out += *p;
    if (chatterMs && g.accept(static_cast<uint8_t>(*p), t + chatterMs)) out += *p;
  }
  return out;
}

void test_slow_press_chatter_types_once() {
  TEST_ASSERT_EQUAL_STRING("hello", typeWithChatter("hello", 400, 180).c_str());
}

void test_fast_typing_with_chatter_types_once() {
  TEST_ASSERT_EQUAL_STRING("hello", typeWithChatter("hello", 200, 90).c_str());
}

void test_intended_doubles_survive_at_any_pace() {
  TEST_ASSERT_EQUAL_STRING("bookkeeper", typeWithChatter("bookkeeper", 450, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("bookkeeper", typeWithChatter("bookkeeper", 150, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("bookkeeper", typeWithChatter("bookkeeper", 450, 200).c_str());
}

void test_same_char_inside_default_window_dropped() {
  M9KeyGuard g;
  TEST_ASSERT_TRUE(g.accept('a', 1000));
  TEST_ASSERT_FALSE(g.accept('a', 1200));
  TEST_ASSERT_TRUE(g.accept('a', 1250));
}

void test_dropped_repeat_does_not_stretch_window() {
  M9KeyGuard g;
  TEST_ASSERT_TRUE(g.accept('a', 1000));
  TEST_ASSERT_FALSE(g.accept('a', 1100));
  TEST_ASSERT_FALSE(g.accept('a', 1200));
  TEST_ASSERT_TRUE(g.accept('a', 1260));
}

void test_navigation_keys_use_short_window() {
  M9KeyGuard g;
  const uint8_t down = 0xB6, backspace = 0x08;
  TEST_ASSERT_TRUE(g.accept(down, 1000));
  TEST_ASSERT_FALSE(g.accept(down, 1050));
  TEST_ASSERT_TRUE(g.accept(down, 1100));
  TEST_ASSERT_TRUE(g.accept(backspace, 1110));
  TEST_ASSERT_TRUE(g.accept(backspace, 1210));
}

void test_different_key_always_accepted() {
  M9KeyGuard g;
  TEST_ASSERT_TRUE(g.accept('a', 1000));
  TEST_ASSERT_TRUE(g.accept('b', 1010));
  TEST_ASSERT_TRUE(g.accept('a', 1020));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_slow_press_chatter_types_once);
  RUN_TEST(test_fast_typing_with_chatter_types_once);
  RUN_TEST(test_intended_doubles_survive_at_any_pace);
  RUN_TEST(test_same_char_inside_default_window_dropped);
  RUN_TEST(test_dropped_repeat_does_not_stretch_window);
  RUN_TEST(test_navigation_keys_use_short_window);
  RUN_TEST(test_different_key_always_accepted);
  return UNITY_END();
}
