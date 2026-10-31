#include <unity.h>

#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/widgets/canned_picker.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static CannedPicker::Out press(CannedPicker& p, Action a) { return p.key(KeyEvent{a}); }

static void expectKind(CannedPicker::Kind want, CannedPicker::Out got) {
  TEST_ASSERT_EQUAL(static_cast<int>(want), static_cast<int>(got.kind));
}

void test_phrases_then_emoji_row() {
  CannedPicker p;
  p.open({"brb", "On my way"});
  TEST_ASSERT_TRUE(p.isOpen());
  TEST_ASSERT_EQUAL(3, p.rows());  // two phrases and "Emoji…"
  expectKind(CannedPicker::Kind::None, press(p, Action::Down));
  CannedPicker::Out o = press(p, Action::Select);
  expectKind(CannedPicker::Kind::Send, o);
  TEST_ASSERT_TRUE(o.text == U"On my way");
  TEST_ASSERT_FALSE(p.isOpen());
}

void test_emoji_row_hands_over_to_the_tray() {
  CannedPicker p;
  p.open({"brb"});
  press(p, Action::Down);
  expectKind(CannedPicker::Kind::Emoji, press(p, Action::Select));
  TEST_ASSERT_FALSE(p.isOpen());
}

void test_back_cancels_and_left_right_do_not() {
  CannedPicker p;
  p.open({"brb"});
  expectKind(CannedPicker::Kind::None, press(p, Action::Left));
  expectKind(CannedPicker::Kind::None, press(p, Action::Right));
  TEST_ASSERT_TRUE(p.isOpen());
  expectKind(CannedPicker::Kind::Closed, press(p, Action::Back));
  TEST_ASSERT_FALSE(p.isOpen());
}

void test_no_phrases_opens_on_emoji() {
  CannedPicker p;
  p.open({});
  TEST_ASSERT_EQUAL(1, p.rows());
  expectKind(CannedPicker::Kind::Emoji, press(p, Action::Select));
}

void test_hold_confirm_is_ignored() {
  CannedPicker p;
  p.open({"brb"});
  expectKind(CannedPicker::Kind::None, press(p, Action::Palette));
  TEST_ASSERT_TRUE(p.isOpen());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_phrases_then_emoji_row);
  RUN_TEST(test_emoji_row_hands_over_to_the_tray);
  RUN_TEST(test_back_cancels_and_left_right_do_not);
  RUN_TEST(test_no_phrases_opens_on_emoji);
  RUN_TEST(test_hold_confirm_is_ignored);
  return UNITY_END();
}
