#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_tanmatsu.h"

using namespace rc;

void setUp() {}
void tearDown() {}

// Bit numbers, byte * 8 + bit, as the coprocessor reports them.
enum : int {
  ESC = 0, F1 = 1, F2 = 2, F3 = 3, TILDE = 4, K1 = 5, K2 = 6, K3 = 7,
  TAB = 8, Q = 9, W = 10, E = 11, FN = 12, A = 13, S = 14, D = 15,
  SHIFT_L = 16, Z = 17, X = 18, C = 19, CTRL = 20, META = 21, ALT_L = 22, BSLASH = 23,
  G = 33,
  F4 = 40, F5 = 41, F6 = 42, BACKSPACE = 43, K9 = 44, K0 = 45, MINUS = 46,
  SEMICOLON = 53, RETURN = 55,
  DOT = 56, SLASH = 57, UP = 58, SHIFT_R = 59, ALT_R = 60, LEFT = 61, DOWN = 62, RIGHT = 63,
  K8 = 64, I = 65, SPACE_L = 68, SPACE_M = 69, SPACE_R = 70, VOL_UP = 71,
};

struct Keys {
  uint8_t s[kTanmatsuKeyBytes] = {};
  Keys& down(int b) {
    s[b / 8] |= uint8_t(1u << (b % 8));
    return *this;
  }
};

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char32_t c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)c, (uint32_t)e.ch);
}

// With `held` down, presses b; returns its event, then lets everything go.
static KeyEvent tap(TanmatsuKeys& k, Keys held, int b, uint32_t ms = 0) {
  k.update(held.s, ms);
  while (k.next(ms).action != Action::None) {
  }
  Keys withKey = held;
  k.update(withKey.down(b).s, ms);
  const KeyEvent e = k.next(ms);
  k.update(Keys{}.s, ms);
  while (k.next(ms).action != Action::None) {
  }
  return e;
}

void test_letters_and_digits() {
  TanmatsuKeys k;
  expectChar(tap(k, {}, Q), U'q');
  expectChar(tap(k, {}, A), U'a');
  expectChar(tap(k, {}, I), U'i');
  expectChar(tap(k, {}, K1), U'1');
  expectChar(tap(k, {}, K8), U'8');
  expectChar(tap(k, {}, K0), U'0');
  expectChar(tap(k, {}, TILDE), U'`');
  expectChar(tap(k, {}, BSLASH), U'\\');
}

void test_shift_both_sides() {
  TanmatsuKeys k;
  expectChar(tap(k, Keys{}.down(SHIFT_L), Q), U'Q');
  expectChar(tap(k, Keys{}.down(SHIFT_R), K1), U'!');
  expectChar(tap(k, Keys{}.down(SHIFT_L), MINUS), U'_');
  expectChar(tap(k, Keys{}.down(SHIFT_R), SLASH), U'?');
  // Shift is held, not one-shot: released, it no longer applies.
  expectChar(tap(k, {}, Q), U'q');
}

void test_altgr() {
  TanmatsuKeys k;
  expectChar(tap(k, Keys{}.down(ALT_R), E), U'é');
  expectChar(tap(k, Keys{}.down(ALT_R).down(SHIFT_L), E), U'É');
  // No AltGr character, or only a combining accent: the plain one.
  expectChar(tap(k, Keys{}.down(ALT_R), G), U'g');
  expectChar(tap(k, Keys{}.down(ALT_R), SEMICOLON), U';');
}

void test_spaces() {
  TanmatsuKeys k;
  expectChar(tap(k, {}, SPACE_L), U' ');
  expectChar(tap(k, {}, SPACE_M), U' ');
  expectChar(tap(k, {}, SPACE_R), U' ');
}

void test_actions() {
  TanmatsuKeys k;
  expectAction(tap(k, {}, ESC), Action::Back);
  expectAction(tap(k, {}, RETURN), Action::Select);
  expectAction(tap(k, {}, TAB), Action::Palette);
  expectAction(tap(k, {}, BACKSPACE), Action::Backspace);
  expectAction(tap(k, {}, UP), Action::Up);
  expectAction(tap(k, {}, DOWN), Action::Down);
  expectAction(tap(k, {}, LEFT), Action::Left);
  expectAction(tap(k, {}, RIGHT), Action::Right);
  expectAction(tap(k, {}, VOL_UP), Action::Up);
}

void test_function_keys() {
  TanmatsuKeys k;
  expectAction(tap(k, {}, F1), Action::OpenHome);
  expectAction(tap(k, {}, F2), Action::OpenCirc);
  expectAction(tap(k, {}, F3), Action::OpenCmail);
  expectAction(tap(k, {}, F4), Action::OpenFeed);
  expectAction(tap(k, {}, F5), Action::OpenMap);
  expectAction(tap(k, {}, F6), Action::OpenProfile);
}

void test_fn_layer() {
  TanmatsuKeys k;
  const Keys fn = Keys{}.down(FN);
  expectAction(tap(k, fn, F1), Action::OpenSettings);
  expectAction(tap(k, fn, F2), Action::None);
  expectAction(tap(k, fn, F4), Action::KeysHelp);
  expectAction(tap(k, fn, F5), Action::KbLightStep);
  expectAction(tap(k, fn, SPACE_M), Action::Emoji);
  // Fn on its own does nothing.
  TanmatsuKeys k2;
  k2.update(fn.s, 0);
  expectAction(k2.next(0), Action::None);
}

void test_meta_tap_is_home() {
  TanmatsuKeys k;
  k.update(Keys{}.down(META).s, 0);
  expectAction(k.next(0), Action::None);
  k.update(Keys{}.s, 50);
  expectAction(k.next(50), Action::OpenHome);
}

void test_meta_with_key_types_nothing() {
  TanmatsuKeys k;
  k.update(Keys{}.down(META).s, 0);
  k.update(Keys{}.down(META).down(Q).s, 10);
  expectAction(k.next(10), Action::None);
  k.update(Keys{}.down(META).s, 20);
  k.update(Keys{}.s, 30);
  expectAction(k.next(30), Action::None);
}

void test_ctrl_tap_is_emoji_or_palette() {
  TanmatsuKeys k;
  k.update(Keys{}.down(CTRL).s, 0);
  expectAction(k.next(0), Action::None);
  k.update(Keys{}.s, 50);
  expectAction(k.next(50), Action::Emoji);
}

void test_ctrl_with_key_is_not_a_tap() {
  TanmatsuKeys k;
  k.update(Keys{}.down(CTRL).s, 0);
  k.update(Keys{}.down(CTRL).down(UP).s, 10);
  expectAction(k.next(10), Action::Up);
  k.update(Keys{}.down(CTRL).s, 20);
  k.update(Keys{}.s, 30);
  expectAction(k.next(30), Action::None);
}

void test_ctrl_and_alt_swallow_chars() {
  TanmatsuKeys k;
  expectAction(tap(k, Keys{}.down(CTRL), Q), Action::None);
  expectAction(tap(k, Keys{}.down(ALT_L), Q), Action::None);
  // Actions still go through.
  expectAction(tap(k, Keys{}.down(CTRL), UP), Action::Up);
}

void test_several_in_one_update() {
  TanmatsuKeys k;
  k.update(Keys{}.down(Q).down(W).s, 0);
  expectChar(k.next(0), U'q');
  expectChar(k.next(0), U'w');
  expectAction(k.next(0), Action::None);
}

void test_held_letter_does_not_repeat_or_retype() {
  TanmatsuKeys k;
  k.update(Keys{}.down(Q).s, 0);
  expectChar(k.next(0), U'q');
  k.update(Keys{}.down(Q).s, 1000);
  expectAction(k.next(1000), Action::None);
}

void test_backspace_repeat() {
  TanmatsuKeys k;
  k.update(Keys{}.down(BACKSPACE).s, 0);
  expectAction(k.next(0), Action::Backspace);
  expectAction(k.next(kTanmatsuRepeatDelayMs - 1), Action::None);
  expectAction(k.next(kTanmatsuRepeatDelayMs), Action::Backspace);
  expectAction(k.next(kTanmatsuRepeatDelayMs + kTanmatsuRepeatMs - 1), Action::None);
  expectAction(k.next(kTanmatsuRepeatDelayMs + kTanmatsuRepeatMs), Action::Backspace);
  k.update(Keys{}.s, 2000);
  expectAction(k.next(5000), Action::None);
}

void test_arrow_repeat_follows_last_pressed() {
  TanmatsuKeys k;
  k.update(Keys{}.down(UP).s, 0);
  expectAction(k.next(0), Action::Up);
  k.update(Keys{}.down(UP).down(DOWN).s, 100);
  expectAction(k.next(100), Action::Down);
  expectAction(k.next(100 + kTanmatsuRepeatDelayMs), Action::Down);
  // Releasing the older key keeps the newer one repeating.
  k.update(Keys{}.down(DOWN).s, 1000);
  expectAction(k.next(1000 + kTanmatsuRepeatMs), Action::Down);
}

void test_function_key_table() {
  TEST_ASSERT_EQUAL(11, (int)kTanmatsuFunctionKeyCount);
  expectAction(KeyEvent{kTanmatsuFunctionKeys[10].action}, Action::Palette);
  expectAction(KeyEvent{kTanmatsuFunctionKeys[9].action}, Action::Emoji);
  expectAction(KeyEvent{kTanmatsuFunctionKeys[6].action}, Action::OpenSettings);
  TEST_ASSERT_TRUE(kTanmatsuFunctionKeys[6].key[0] == U'F' && kTanmatsuFunctionKeys[6].key[1] == U'n');
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_letters_and_digits);
  RUN_TEST(test_shift_both_sides);
  RUN_TEST(test_altgr);
  RUN_TEST(test_spaces);
  RUN_TEST(test_actions);
  RUN_TEST(test_function_keys);
  RUN_TEST(test_fn_layer);
  RUN_TEST(test_meta_tap_is_home);
  RUN_TEST(test_meta_with_key_types_nothing);
  RUN_TEST(test_ctrl_tap_is_emoji_or_palette);
  RUN_TEST(test_ctrl_with_key_is_not_a_tap);
  RUN_TEST(test_ctrl_and_alt_swallow_chars);
  RUN_TEST(test_several_in_one_update);
  RUN_TEST(test_held_letter_does_not_repeat_or_retype);
  RUN_TEST(test_backspace_repeat);
  RUN_TEST(test_arrow_repeat_follows_last_pressed);
  RUN_TEST(test_function_key_table);
  return UNITY_END();
}
