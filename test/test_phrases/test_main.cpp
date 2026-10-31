#include <unity.h>

#include <string>
#include <vector>

#include "config/phrases.h"
#include "power/power_policy.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_clean_trims_drops_and_caps() {
  std::vector<std::string> in = {"  hi  ", "", "   ", "a\tb\nc", "x"};
  auto out = cleanPhrases(in);
  TEST_ASSERT_EQUAL(3, out.size());
  TEST_ASSERT_EQUAL_STRING("hi", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("a b c", out[1].c_str());
  TEST_ASSERT_EQUAL_STRING("x", out[2].c_str());

  std::vector<std::string> many(15, "p");
  TEST_ASSERT_EQUAL(kMaxPhrases, cleanPhrases(many).size());
}

void test_clean_truncates_on_a_character_boundary() {
  std::string s(kMaxPhraseBytes - 1, 'a');
  s += "é";  // two bytes: one over
  auto out = cleanPhrases({s});
  TEST_ASSERT_EQUAL(kMaxPhraseBytes - 1, out[0].size());
}

void test_join_split_round_trip() {
  std::vector<std::string> p = {"On my way", "Thanks! 👍", "brb"};
  TEST_ASSERT_EQUAL_STRING("On my way\nThanks! 👍\nbrb", joinPhrases(p).c_str());
  auto back = splitPhrases("On my way\r\nThanks! 👍\n\nbrb\n");
  TEST_ASSERT_EQUAL(3, back.size());
  TEST_ASSERT_EQUAL_STRING("Thanks! 👍", back[1].c_str());
}

void test_defaults_are_clean() {
  auto d = defaultPhrases();
  TEST_ASSERT_TRUE(d.size() > 0 && d.size() <= kMaxPhrases);
  TEST_ASSERT_EQUAL(d.size(), cleanPhrases(d).size());
}

void test_idle_and_wake_choices() {
  TEST_ASSERT_TRUE(validIdleSecs(300));
  TEST_ASSERT_TRUE(validIdleSecs(60));
  TEST_ASSERT_FALSE(validIdleSecs(0));
  TEST_ASSERT_FALSE(validIdleSecs(7));
  TEST_ASSERT_TRUE(validWakeMins(15));
  TEST_ASSERT_TRUE(validWakeMins(0));  // never: sleep until a button
  TEST_ASSERT_FALSE(validWakeMins(16));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_clean_trims_drops_and_caps);
  RUN_TEST(test_clean_truncates_on_a_character_boundary);
  RUN_TEST(test_join_split_round_trip);
  RUN_TEST(test_defaults_are_clean);
  RUN_TEST(test_idle_and_wake_choices);
  return UNITY_END();
}
