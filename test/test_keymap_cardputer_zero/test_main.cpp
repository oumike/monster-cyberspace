// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_cardputer_zero.h"

using namespace rc;
using namespace rc::zerokey;

void setUp() {}
void tearDown() {}

static void expectAction(Action a, Action got) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(got));
}

// A press (value 1) at ms; true and the event if it gave one.
static bool press(CardputerZeroKeys& k, uint16_t code, uint32_t ms, KeyEvent& out,
                  uint8_t mods = 0) {
  return k.feed(ZeroKeyIn{code, 1, ms, mods}, out);
}

static bool release(CardputerZeroKeys& k, uint16_t code, uint32_t ms) {
  KeyEvent ignored;
  return k.feed(ZeroKeyIn{code, 0, ms, 0}, ignored);
}

static KeyEvent tap(CardputerZeroKeys& k, uint16_t code, uint8_t mods = 0) {
  KeyEvent e;
  if (!press(k, code, 0, e, mods)) e = KeyEvent{};
  release(k, code, 1);
  return e;
}

static void expectChar(char32_t c, const KeyEvent& e) {
  expectAction(Action::Char, e.action);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)c, (uint32_t)e.ch);
}

void test_letters_and_digits() {
  CardputerZeroKeys k;
  const struct { uint16_t code; char c; } table[] = {
      {kQ, 'q'}, {kW, 'w'}, {kE, 'e'}, {kR, 'r'}, {kT, 't'}, {kY, 'y'}, {kU, 'u'},
      {kI, 'i'}, {kO, 'o'}, {kP, 'p'}, {kA, 'a'}, {kS, 's'}, {kD, 'd'}, {kF, 'f'},
      {kG, 'g'}, {kH, 'h'}, {kJ, 'j'}, {kK, 'k'}, {kL, 'l'}, {kZ, 'z'}, {kX, 'x'},
      {kC, 'c'}, {kV, 'v'}, {kB, 'b'}, {kN, 'n'}, {kM, 'm'}, {k1, '1'}, {k2, '2'},
      {k3, '3'}, {k4, '4'}, {k5, '5'}, {k6, '6'}, {k7, '7'}, {k8, '8'}, {k9, '9'},
      {k0, '0'}, {kSpace, ' '}, {kMinus, '-'}, {kEqual, '='},
  };
  for (const auto& t : table) expectChar(char32_t(t.c), tap(k, t.code));
}

void test_held_shift() {
  CardputerZeroKeys k;
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kLeftShift, 0, e));
  expectChar(U'A', tap(k, kA));
  expectChar(U'!', tap(k, k1));
  expectChar(U'(', tap(k, k9));
  expectChar(U'_', tap(k, kMinus));
  TEST_ASSERT_FALSE(release(k, kLeftShift, 5));
  expectChar(U'a', tap(k, kA));
  press(k, kRightShift, 10, e);
  expectChar(U'Z', tap(k, kZ));
}

void test_modifier_state_shift() {
  // A one-shot or locked shift the kernel never reports as KEY_LEFTSHIFT.
  CardputerZeroKeys k;
  expectChar(U'Q', tap(k, kQ, kZeroModShift));
  expectChar(U'@', tap(k, k2, kZeroModShift));
  expectChar(U'q', tap(k, kQ));
}

void test_builtin_sym_table() {
  CardputerZeroKeys k;
  const struct { uint16_t code; char c; } table[] = {
      {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'}, {51, '&'},
      {52, '*'}, {53, '('}, {94, ')'}, {55, '~'}, {69, '`'}, {70, '_'}, {71, '-'},
      {72, '+'}, {73, '='}, {74, '['}, {75, ']'}, {76, '{'}, {77, '}'}, {79, ';'},
      {80, ':'}, {81, '\''}, {82, '"'}, {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'},
      {90, ','}, {91, '.'}, {92, '/'}, {93, '?'},
  };
  for (const auto& t : table) expectChar(char32_t(t.c), tap(k, t.code));
  // Shift doesn't change a sym-layer character.
  expectChar(U',', tap(k, 90, kZeroModShift));
}

void test_loaded_sym_map() {
  CardputerZeroKeys k;
  const char* file =
      "# tca8418 keymap\n"
      "keycode 26 = question   # remapped\n"
      "\n"
      "keycode 90 = U20AC\n"
      "keycode 91 = notakeysym\n"
      "garbage line\n"
      "keycode = comma\n"
      "  keycode 92 = x\n";
  TEST_ASSERT_TRUE(k.loadSymMap(file));
  expectChar(U'?', tap(k, 26));
  expectChar(U'€', tap(k, 90));
  expectChar(U'x', tap(k, 92));
  // The file replaces the built-in table: 27 and the unknown keysym type nothing.
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, 27, 0, e));
  TEST_ASSERT_FALSE(press(k, 91, 0, e));
}

void test_empty_sym_map_keeps_builtin() {
  CardputerZeroKeys k;
  TEST_ASSERT_FALSE(k.loadSymMap("# nothing here\nbad\n"));
  expectChar(U'!', tap(k, 26));
}

void test_arrows_and_navigation() {
  CardputerZeroKeys k;
  expectAction(Action::Up, tap(k, kUp).action);
  expectAction(Action::Down, tap(k, kDown).action);
  expectAction(Action::Left, tap(k, kLeft).action);
  expectAction(Action::Right, tap(k, kRight).action);
  expectAction(Action::OpenHome, tap(k, kHome).action);
  expectAction(Action::Up, tap(k, kPageUp).action);
  expectAction(Action::Down, tap(k, kPageDown).action);
  expectAction(Action::Backspace, tap(k, kDelete).action);
  expectAction(Action::KeysHelp, tap(k, kHelp).action);
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kEnd, 0, e));
}

void test_fn_fallback_on_raw_letters() {
  // The simulator (and a kernel that left a key unmapped) sends KEY_FN + letter.
  CardputerZeroKeys k;
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kFn, 0, e));
  expectAction(Action::Up, tap(k, kF).action);
  expectAction(Action::Down, tap(k, kX).action);
  expectAction(Action::Left, tap(k, kZ).action);
  expectAction(Action::Right, tap(k, kC).action);
  expectAction(Action::OpenHome, tap(k, kK).action);
  expectAction(Action::Up, tap(k, kL).action);
  expectAction(Action::Down, tap(k, kM).action);
  expectAction(Action::Backspace, tap(k, kBackspace).action);
  expectAction(Action::OpenHome, tap(k, k1).action);
  expectAction(Action::OpenSettings, tap(k, k7).action);
  expectAction(Action::KeysHelp, tap(k, k0).action);
  expectAction(Action::KeysHelp, tap(k, kH).action);
  TEST_ASSERT_FALSE(press(k, kQ, 10, e));
  release(k, kFn, 20);
  expectChar(U'f', tap(k, kF));
  // The modifier_state fn bit works the same.
  expectAction(Action::Up, tap(k, kF, kZeroModFn).action);
}

void test_function_keys() {
  CardputerZeroKeys k;
  const uint16_t codes[] = {kF1, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10};
  for (int i = 0; i < 10; ++i) expectAction(functionKeyAction(i + 1), tap(k, codes[i]).action);
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kF11, 0, e));
  TEST_ASSERT_FALSE(press(k, kF12, 0, e));
}

void test_enter_tab_backspace() {
  CardputerZeroKeys k;
  expectAction(Action::Select, tap(k, kEnter).action);
  expectAction(Action::Select, tap(k, kKpEnter).action);
  expectAction(Action::Palette, tap(k, kTab).action);
  expectAction(Action::Backspace, tap(k, kBackspace).action);
}

void test_short_esc_is_back() {
  CardputerZeroKeys k;
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kEsc, 0, e));
  TEST_ASSERT_TRUE(k.feed(ZeroKeyIn{kEsc, 0, 400, 0}, e));
  expectAction(Action::Back, e.action);
  TEST_ASSERT_FALSE(k.quitRequested());
}

void test_long_esc_quits() {
  CardputerZeroKeys k;
  KeyEvent e;
  press(k, kEsc, 0, e);
  TEST_ASSERT_FALSE(k.poll(999, e));
  TEST_ASSERT_FALSE(k.quitRequested());
  TEST_ASSERT_FALSE(k.poll(1000, e));
  TEST_ASSERT_TRUE(k.quitRequested());
  TEST_ASSERT_FALSE(k.feed(ZeroKeyIn{kEsc, 0, 1200, 0}, e));
  TEST_ASSERT_TRUE(k.quitRequested());
}

void test_long_esc_seen_on_release() {
  // No poll while held: the release still knows how long it was.
  CardputerZeroKeys k;
  KeyEvent e;
  press(k, kEsc, 0, e);
  TEST_ASSERT_FALSE(k.feed(ZeroKeyIn{kEsc, 0, 1500, 0}, e));
  TEST_ASSERT_TRUE(k.quitRequested());
}

void test_arrows_and_backspace_repeat() {
  CardputerZeroKeys k;
  KeyEvent e;
  TEST_ASSERT_TRUE(press(k, kDown, 0, e));
  TEST_ASSERT_FALSE(k.poll(kZeroRepeatDelayMs - 1, e));
  TEST_ASSERT_TRUE(k.poll(kZeroRepeatDelayMs, e));
  expectAction(Action::Down, e.action);
  TEST_ASSERT_FALSE(k.poll(kZeroRepeatDelayMs + kZeroRepeatMs - 1, e));
  TEST_ASSERT_TRUE(k.poll(kZeroRepeatDelayMs + kZeroRepeatMs, e));
  // The kernel's own repeats are ignored: the keymap times them.
  TEST_ASSERT_FALSE(k.feed(ZeroKeyIn{kDown, 2, 700, 0}, e));
  release(k, kDown, 800);
  TEST_ASSERT_FALSE(k.poll(2000, e));

  TEST_ASSERT_TRUE(press(k, kBackspace, 3000, e));
  TEST_ASSERT_TRUE(k.poll(3000 + kZeroRepeatDelayMs, e));
  expectAction(Action::Backspace, e.action);
  release(k, kBackspace, 4000);

  // Letters don't repeat.
  TEST_ASSERT_TRUE(press(k, kA, 5000, e));
  TEST_ASSERT_FALSE(k.poll(6000, e));
}

void test_system_keys() {
  CardputerZeroKeys k;
  KeyEvent e;
  TEST_ASSERT_FALSE(press(k, kVolumeUp, 0, e));
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::VolumeUp), int(k.takeSystemKey()));
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::None), int(k.takeSystemKey()));
  press(k, kVolumeDown, 0, e);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::VolumeDown), int(k.takeSystemKey()));
  press(k, kMute, 0, e);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::Mute), int(k.takeSystemKey()));
  press(k, kBrightnessUp, 0, e);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::BrightnessUp), int(k.takeSystemKey()));
  press(k, kBrightnessDown, 0, e);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::BrightnessDown), int(k.takeSystemKey()));
  // fn+U/I and fn+S/D as raw letters.
  press(k, kI, 0, e, kZeroModFn);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::BrightnessUp), int(k.takeSystemKey()));
  press(k, kU, 0, e, kZeroModFn);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::BrightnessDown), int(k.takeSystemKey()));
  press(k, kD, 0, e, kZeroModFn);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::VolumeUp), int(k.takeSystemKey()));
  press(k, kS, 0, e, kZeroModFn);
  TEST_ASSERT_EQUAL(int(ZeroSystemKey::VolumeDown), int(k.takeSystemKey()));
}

void test_modifiers_unknown_and_releases_send_nothing() {
  CardputerZeroKeys k;
  KeyEvent e;
  const uint16_t mods[] = {kLeftShift, kRightShift, kFn, kCompose, kLeftCtrl, kLeftAlt};
  for (uint16_t m : mods) TEST_ASSERT_FALSE(press(k, m, 0, e));
  CardputerZeroKeys fresh;
  TEST_ASSERT_FALSE(press(fresh, 600, 0, e));
  TEST_ASSERT_FALSE(press(fresh, kSysRq, 0, e));
  TEST_ASSERT_FALSE(release(fresh, kA, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_letters_and_digits);
  RUN_TEST(test_held_shift);
  RUN_TEST(test_modifier_state_shift);
  RUN_TEST(test_builtin_sym_table);
  RUN_TEST(test_loaded_sym_map);
  RUN_TEST(test_empty_sym_map_keeps_builtin);
  RUN_TEST(test_arrows_and_navigation);
  RUN_TEST(test_fn_fallback_on_raw_letters);
  RUN_TEST(test_function_keys);
  RUN_TEST(test_enter_tab_backspace);
  RUN_TEST(test_short_esc_is_back);
  RUN_TEST(test_long_esc_quits);
  RUN_TEST(test_long_esc_seen_on_release);
  RUN_TEST(test_arrows_and_backspace_repeat);
  RUN_TEST(test_system_keys);
  RUN_TEST(test_modifiers_unknown_and_releases_send_nothing);
  return UNITY_END();
}
