// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_tab5.h"

using namespace rc;

void setUp() {}
void tearDown() {}

// Row and column of a key on the Tab5 Keyboard's 5x14 matrix.
struct RC {
  uint8_t row, col;
};
constexpr RC ESC{0, 0}, K1{0, 1}, K2{0, 2}, K7{0, 7}, K8{0, 8}, K0{0, 10}, PLUS{0, 12}, DEL{0, 13};
constexpr RC GRAVE{1, 0}, BANG{1, 1}, AT{1, 2}, STAR{1, 8}, LPAREN{1, 9}, PIPE{1, 13};
constexpr RC TAB{2, 0}, Q{2, 1}, SEMI{2, 11}, QUOTE{2, 12}, BKSP{2, 13};
constexpr RC SYM{3, 0}, AA{3, 1}, A{3, 2}, UP{3, 11}, UNDER{3, 12}, ENTER{3, 13};
constexpr RC CTRL{4, 0}, ALT{4, 1}, M{4, 8}, DOT{4, 9}, LEFT{4, 10}, DOWN{4, 11}, RIGHT{4, 12},
    SPACE{4, 13};

uint8_t ev(RC k, bool pressed) { return uint8_t((pressed ? 0x80 : 0) | k.row << 4 | k.col); }

KeyEvent press(Tab5Keys& k, RC key, uint32_t ms = 0) { return k.key(ev(key, true), ms); }
KeyEvent release(Tab5Keys& k, RC key, uint32_t ms = 0) { return k.key(ev(key, false), ms); }
KeyEvent tap(Tab5Keys& k, RC key, uint32_t ms = 0) {
  const KeyEvent e = press(k, key, ms);
  release(k, key, ms);
  return e;
}

void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

void expectChar(const KeyEvent& e, char32_t c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL_UINT32(uint32_t(c), uint32_t(e.ch));
}

void test_letters_digits_symbols() {
  Tab5Keys k;
  expectChar(tap(k, Q), U'q');
  expectChar(tap(k, A), U'a');
  expectChar(tap(k, M), U'm');
  expectChar(tap(k, K1), U'1');
  expectChar(tap(k, K0), U'0');
  expectChar(tap(k, PLUS), U'+');
  expectChar(tap(k, BANG), U'!');
  expectChar(tap(k, STAR), U'*');
  expectChar(tap(k, SEMI), U';');
  expectChar(tap(k, UNDER), U'_');
  expectChar(tap(k, DOT), U'.');
  expectChar(tap(k, SPACE), U' ');
}

void test_releases_and_bad_events_make_nothing() {
  Tab5Keys k;
  press(k, Q);
  expectAction(release(k, Q), Action::None);
  expectAction(k.key(0xFF, 0), Action::None);
  expectAction(k.key(uint8_t(0x80 | 5 << 4 | 0), 0), Action::None);  // row 5
  expectAction(k.key(uint8_t(0x80 | 0 << 4 | 14), 0), Action::None);  // column 14
}

void test_sym_held_gives_second_layer() {
  Tab5Keys k;
  press(k, SYM);
  expectChar(tap(k, GRAVE), U'~');
  expectChar(tap(k, BANG), U'?');
  expectChar(tap(k, STAR), U'/');
  expectChar(tap(k, LPAREN), U'<');
  expectChar(tap(k, PIPE), U'|');
  expectChar(tap(k, SEMI), U':');
  expectChar(tap(k, QUOTE), U'"');
  expectChar(tap(k, UNDER), U'=');
  expectChar(tap(k, DOT), U',');
  expectChar(tap(k, AT), U'@');  // no second layer
  expectChar(tap(k, Q), U'q');
  expectAction(release(k, SYM), Action::None);
  expectChar(tap(k, DOT), U'.');
}

void test_sym_tapped_alone_is_one_shot() {
  Tab5Keys k;
  tap(k, SYM);
  expectChar(tap(k, DOT), U',');
  expectChar(tap(k, DOT), U'.');
  tap(k, SYM);
  tap(k, SYM);  // a second tap disarms
  expectChar(tap(k, DOT), U'.');
}

void test_aa_tap_is_one_shot() {
  Tab5Keys k;
  tap(k, AA, 0);
  TEST_ASSERT_TRUE(k.shiftPending());
  expectChar(tap(k, Q, 100), U'Q');
  TEST_ASSERT_FALSE(k.shiftPending());
  expectChar(tap(k, Q, 200), U'q');
  tap(k, AA, 1000);
  expectChar(tap(k, SEMI, 1100), U':');  // Shift gives the second layer
  expectChar(tap(k, SEMI, 1200), U';');
}

void test_aa_double_tap_is_caps_lock() {
  Tab5Keys k;
  tap(k, AA, 0);
  tap(k, AA, 300);
  TEST_ASSERT_TRUE(k.capsLock());
  TEST_ASSERT_FALSE(k.shiftPending());
  expectChar(tap(k, Q, 400), U'Q');
  expectChar(tap(k, A, 500), U'A');
  expectChar(tap(k, SEMI, 600), U';');  // letters only
  expectChar(tap(k, K1, 700), U'1');
  tap(k, AA, 2000);  // a tap ends it
  TEST_ASSERT_FALSE(k.capsLock());
  TEST_ASSERT_FALSE(k.shiftPending());
  expectChar(tap(k, Q, 2100), U'q');
}

void test_aa_slow_second_tap_is_not_caps_lock() {
  Tab5Keys k;
  tap(k, AA, 0);
  tap(k, AA, 800);
  TEST_ASSERT_FALSE(k.capsLock());
  TEST_ASSERT_TRUE(k.shiftPending());
}

void test_aa_held_is_shift_while_held() {
  Tab5Keys k;
  press(k, AA, 0);
  expectChar(tap(k, Q, 100), U'Q');
  expectChar(tap(k, A, 200), U'A');
  release(k, AA, 300);
  TEST_ASSERT_FALSE(k.shiftPending());
  TEST_ASSERT_FALSE(k.capsLock());
  expectChar(tap(k, Q, 400), U'q');
  // A long hold alone arms nothing either.
  press(k, AA, 1000);
  release(k, AA, 2500);
  TEST_ASSERT_FALSE(k.shiftPending());
}

void test_actions() {
  Tab5Keys k;
  expectAction(tap(k, ESC), Action::Back);
  expectAction(tap(k, ENTER), Action::Select);
  expectAction(tap(k, TAB), Action::Palette);
  expectAction(tap(k, BKSP), Action::Backspace);
  expectAction(tap(k, DEL), Action::Backspace);
  expectAction(tap(k, UP), Action::Up);
  expectAction(tap(k, DOWN), Action::Down);
  expectAction(tap(k, LEFT), Action::Left);
  expectAction(tap(k, RIGHT), Action::Right);
}

void test_modifiers_alone_make_nothing_on_press() {
  Tab5Keys k;
  expectAction(press(k, SYM), Action::None);
  expectAction(press(k, AA), Action::None);
  expectAction(press(k, CTRL), Action::None);
  expectAction(press(k, ALT), Action::None);
}

void test_ctrl_tap_is_emoji() {
  Tab5Keys k;
  press(k, CTRL);
  expectAction(release(k, CTRL), Action::Emoji);
}

void test_ctrl_with_key_types_nothing_and_is_not_a_tap() {
  Tab5Keys k;
  press(k, CTRL);
  expectAction(tap(k, Q), Action::None);
  expectAction(tap(k, LEFT), Action::Left);  // actions pass
  expectAction(release(k, CTRL), Action::None);
  expectChar(tap(k, Q), U'q');
}

void test_alt_digits_are_function_keys() {
  Tab5Keys k;
  press(k, ALT);
  expectAction(tap(k, K1), Action::OpenHome);
  expectAction(tap(k, K2), Action::OpenCirc);
  expectAction(tap(k, K7), Action::OpenSettings);
  expectAction(tap(k, K8), Action::None);
  expectAction(tap(k, K0), Action::KeysHelp);
  expectAction(tap(k, Q), Action::None);
  expectAction(release(k, ALT), Action::None);
  expectChar(tap(k, K1), U'1');
}

void test_backspace_repeats_until_released() {
  Tab5Keys k;
  expectAction(press(k, BKSP, 0), Action::Backspace);
  expectAction(k.poll(kTab5RepeatDelayMs - 1), Action::None);
  expectAction(k.poll(kTab5RepeatDelayMs), Action::Backspace);
  expectAction(k.poll(kTab5RepeatDelayMs + 1), Action::None);
  expectAction(k.poll(kTab5RepeatDelayMs + kTab5RepeatMs), Action::Backspace);
  release(k, BKSP, 1000);
  expectAction(k.poll(5000), Action::None);
}

void test_arrow_repeat_follows_last_pressed() {
  Tab5Keys k;
  press(k, UP, 0);
  press(k, DOWN, 100);
  expectAction(k.poll(100 + kTab5RepeatDelayMs), Action::Down);
  release(k, UP, 600);  // not the repeating key
  expectAction(k.poll(100 + kTab5RepeatDelayMs + kTab5RepeatMs), Action::Down);
}

void test_letters_do_not_repeat() {
  Tab5Keys k;
  press(k, Q, 0);
  expectAction(k.poll(5000), Action::None);
}

void test_function_key_table() {
  TEST_ASSERT_EQUAL(10, int(kTab5FunctionKeyCount));
  TEST_ASSERT_EQUAL(static_cast<int>(Action::OpenHome),
                    static_cast<int>(kTab5FunctionKeys[0].action));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_letters_digits_symbols);
  RUN_TEST(test_releases_and_bad_events_make_nothing);
  RUN_TEST(test_sym_held_gives_second_layer);
  RUN_TEST(test_sym_tapped_alone_is_one_shot);
  RUN_TEST(test_aa_tap_is_one_shot);
  RUN_TEST(test_aa_double_tap_is_caps_lock);
  RUN_TEST(test_aa_slow_second_tap_is_not_caps_lock);
  RUN_TEST(test_aa_held_is_shift_while_held);
  RUN_TEST(test_actions);
  RUN_TEST(test_modifiers_alone_make_nothing_on_press);
  RUN_TEST(test_ctrl_tap_is_emoji);
  RUN_TEST(test_ctrl_with_key_types_nothing_and_is_not_a_tap);
  RUN_TEST(test_alt_digits_are_function_keys);
  RUN_TEST(test_backspace_repeats_until_released);
  RUN_TEST(test_arrow_repeat_follows_last_pressed);
  RUN_TEST(test_letters_do_not_repeat);
  RUN_TEST(test_function_key_table);
  return UNITY_END();
}
