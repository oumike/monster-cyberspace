#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_tdisplay_p4.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL((uint32_t)c, (uint32_t)e.ch);
}

void test_letters() {
  TdisplayP4Keys k;
  expectChar(k.key(21, true, 0), 'q');
  expectChar(k.key(32, true, 0), 'a');
  expectChar(k.key(48, true, 0), 'm');
  expectChar(k.key(56, true, 0), ' ');
}

void test_digits() {
  TdisplayP4Keys k;
  expectChar(k.key(13, true, 0), '1');
  expectChar(k.key(62, true, 0), '9');
  expectChar(k.key(67, true, 0), '0');
}

void test_shift_one_shot() {
  TdisplayP4Keys k;
  k.key(53, true, 0);
  expectChar(k.key(21, true, 10), 'Q');
  expectChar(k.key(21, true, 20), 'q');
}

void test_shift_lapses() {
  TdisplayP4Keys k;
  k.key(53, true, 0);
  expectChar(k.key(21, true, 1501), 'q');
}

void test_sym() {
  TdisplayP4Keys k;
  k.key(51, true, 0);
  expectChar(k.key(39, true, 10), '/');
  k.key(58, true, 20);
  expectChar(k.key(40, true, 30), '?');
}

void test_sym_falls_back_to_base() {
  TdisplayP4Keys k;
  k.key(51, true, 0);
  expectChar(k.key(42, true, 10), 'z');
}

void test_caps_lock() {
  TdisplayP4Keys k;
  k.key(31, true, 0);
  TEST_ASSERT_TRUE(k.capsLock());
  expectChar(k.key(21, true, 10), 'Q');
  expectChar(k.key(21, true, 20), 'Q');
  k.key(31, true, 30);
  TEST_ASSERT_FALSE(k.capsLock());
  expectChar(k.key(21, true, 40), 'q');
}

void test_caps_lock_letters_only() {
  TdisplayP4Keys k;
  k.key(31, true, 0);
  expectChar(k.key(13, true, 10), '1');
  k.key(53, true, 20);
  expectChar(k.key(21, true, 30), 'q');
}

void test_actions() {
  TdisplayP4Keys k;
  expectAction(k.key(11, true, 0), Action::Back);
  expectAction(k.key(64, true, 0), Action::Select);
  expectAction(k.key(54, true, 0), Action::Palette);
  expectAction(k.key(65, true, 0), Action::Emoji);
  expectAction(k.key(50, true, 0), Action::Up);
  expectAction(k.key(60, true, 0), Action::Down);
  expectAction(k.key(59, true, 0), Action::Left);
  expectAction(k.key(68, true, 0), Action::Right);
}

void test_alt_backspace_is_back() {
  TdisplayP4Keys k;
  expectAction(k.key(41, true, 0), Action::None);
  expectAction(k.key(63, true, 10), Action::Back);
  expectAction(k.poll(1000), Action::None);
  expectAction(k.key(63, true, 20), Action::Backspace);
}

void test_backspace_repeat() {
  TdisplayP4Keys k;
  expectAction(k.key(63, true, 0), Action::Backspace);
  expectAction(k.poll(449), Action::None);
  expectAction(k.poll(450), Action::Backspace);
  expectAction(k.poll(519), Action::None);
  expectAction(k.poll(520), Action::Backspace);
  k.key(63, false, 530);
  expectAction(k.poll(600), Action::None);
}

void test_arrow_repeat() {
  TdisplayP4Keys k;
  k.key(60, true, 0);
  expectAction(k.poll(450), Action::Down);
}

void test_f11_light() {
  TdisplayP4Keys k;
  expectAction(k.key(61, true, 0), Action::None);
  TEST_ASSERT_TRUE(k.takeLightStep());
  TEST_ASSERT_FALSE(k.takeLightStep());
}

void test_ignored() {
  TdisplayP4Keys k;
  expectAction(k.key(21, false, 0), Action::None);
  expectAction(k.key(0, true, 0), Action::None);
  expectAction(k.key(69, true, 0), Action::None);
  expectAction(k.key(41, true, 0), Action::None);
  expectAction(k.key(8, true, 0), Action::None);  // F8: unused
}

void test_function_keys() {
  TdisplayP4Keys k;
  expectAction(k.key(1, true, 0), Action::OpenCirc);
  expectAction(k.key(2, true, 0), Action::OpenRooms);
  expectAction(k.key(3, true, 0), Action::OpenCmail);
  expectAction(k.key(4, true, 0), Action::OpenFeed);
  expectAction(k.key(5, true, 0), Action::OpenMap);
  expectAction(k.key(6, true, 0), Action::OpenProfile);
  expectAction(k.key(7, true, 0), Action::OpenSettings);
  expectAction(k.key(9, true, 0), Action::None);
  expectAction(k.key(10, true, 0), Action::KeysHelp);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_letters);
  RUN_TEST(test_digits);
  RUN_TEST(test_shift_one_shot);
  RUN_TEST(test_shift_lapses);
  RUN_TEST(test_sym);
  RUN_TEST(test_sym_falls_back_to_base);
  RUN_TEST(test_caps_lock);
  RUN_TEST(test_caps_lock_letters_only);
  RUN_TEST(test_actions);
  RUN_TEST(test_alt_backspace_is_back);
  RUN_TEST(test_backspace_repeat);
  RUN_TEST(test_arrow_repeat);
  RUN_TEST(test_f11_light);
  RUN_TEST(test_ignored);
  RUN_TEST(test_function_keys);
  return UNITY_END();
}
