#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_tdeck_pro.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint8_t kP = 1, kW = 9, kQ = 10, kBs = 11, kH = 15, kF = 17, kD = 18, kS = 19, kA = 20,
                  kEnter = 21, kDollar = 22, kM = 23, kC = 27, kZ = 29, kAlt = 30, kShiftL = 31,
                  kSym = 32, kSpace = 33, kShiftR = 35;
}

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL((uint32_t)c, (uint32_t)e.ch);
}

void test_letters_and_space() {
  TdeckProKeys k;
  expectChar(k.key(kP, true, 0), 'p');
  expectChar(k.key(kQ, true, 0), 'q');
  expectChar(k.key(kA, true, 0), 'a');
  expectChar(k.key(kZ, true, 0), 'z');
  expectChar(k.key(kM, true, 0), 'm');
  expectChar(k.key(kDollar, true, 0), '$');
  expectChar(k.key(kSpace, true, 0), ' ');
}

void test_release_and_out_of_range_ignored() {
  TdeckProKeys k;
  expectAction(k.key(kP, false, 0), Action::None);
  expectAction(k.key(0, true, 0), Action::None);
  expectAction(k.key(36, true, 0), Action::None);
}

void test_enter_and_backspace() {
  TdeckProKeys k;
  expectAction(k.key(kEnter, true, 0), Action::Select);
  expectAction(k.key(kBs, true, 0), Action::Backspace);
}

void test_shift_one_shot_either_side() {
  TdeckProKeys k;
  expectAction(k.key(kShiftL, true, 0), Action::None);
  expectChar(k.key(kQ, true, 10), 'Q');
  expectChar(k.key(kQ, true, 20), 'q');
  k.key(kShiftR, true, 30);
  expectChar(k.key(kP, true, 40), 'P');
  // A second tap disarms.
  k.key(kShiftL, true, 50);
  k.key(kShiftL, true, 60);
  expectChar(k.key(kP, true, 70), 'p');
}

void test_sym_layer_and_symbols_event() {
  TdeckProKeys k;
  KeyEvent e = k.key(kSym, true, 0);
  expectAction(e, Action::Symbols);
  TEST_ASSERT_TRUE(e.modifier);
  expectChar(k.key(kW, true, 10), '1');
  k.key(kSym, true, 20);
  expectChar(k.key(kP, true, 30), '@');
  k.key(kSym, true, 40);
  expectChar(k.key(34, true, 50), '0');
  k.key(kSym, true, 60);
  expectChar(k.key(kM, true, 70), '.');
  // Shift doesn't upper-case a Sym character.
  k.key(kShiftL, true, 80);
  k.key(kSym, true, 90);
  expectChar(k.key(kA, true, 100), '*');
}

void test_sym_chords() {
  TdeckProKeys k;
  k.key(kSym, true, 0);
  expectAction(k.key(kEnter, true, 10), Action::Palette);
  k.key(kSym, true, 20);
  expectAction(k.key(kBs, true, 30), Action::Back);
  k.key(kSym, true, 40);
  expectAction(k.key(kSpace, true, 50), Action::OpenHome);
}

void test_modifier_times_out() {
  TdeckProKeys k;
  k.key(kShiftL, true, 0);
  expectChar(k.key(kQ, true, kTdeckProModifierMs + 1), 'q');
  k.key(kAlt, true, 0);
  k.key(kAlt, false, 10);
  expectChar(k.key(kW, true, kTdeckProModifierMs + 20), 'w');
}

void test_alt_tap_is_one_shot() {
  TdeckProKeys k;
  expectAction(k.key(kAlt, true, 0), Action::None);
  expectAction(k.key(kAlt, false, 10), Action::None);
  expectAction(k.key(kS, true, 20), Action::Down);
  expectChar(k.key(kS, true, 30), 's');
}

void test_alt_held_chords() {
  TdeckProKeys k;
  k.key(kAlt, true, 0);
  expectAction(k.key(kW, true, 10), Action::Up);
  expectAction(k.key(kA, true, 20), Action::Left);
  expectAction(k.key(kS, true, 30), Action::Down);
  expectAction(k.key(kD, true, 40), Action::Right);
  expectAction(k.key(kBs, true, 50), Action::Back);
  expectAction(k.key(kEnter, true, 60), Action::Palette);
  expectAction(k.key(kH, true, 70), Action::OpenHome);
  expectAction(k.key(kC, true, 80), Action::OpenCirc);
  expectAction(k.key(kM, true, 90), Action::OpenCmail);
  expectAction(k.key(kF, true, 100), Action::OpenFeed);
  // Unmapped letters do nothing under Alt.
  expectAction(k.key(kZ, true, 110), Action::None);
  k.key(kAlt, false, 120);
  expectChar(k.key(kW, true, 130), 'w');
}

void test_backspace_repeats_while_held() {
  TdeckProKeys k;
  expectAction(k.key(kBs, true, 0), Action::Backspace);
  expectAction(k.poll(kTdeckProRepeatDelayMs - 1), Action::None);
  expectAction(k.poll(kTdeckProRepeatDelayMs), Action::Backspace);
  expectAction(k.poll(kTdeckProRepeatDelayMs + 1), Action::None);
  expectAction(k.poll(kTdeckProRepeatDelayMs + kTdeckProRepeatMs), Action::Backspace);
  k.key(kBs, false, 2000);
  expectAction(k.poll(5000), Action::None);
}

void test_alt_arrow_repeats_while_held() {
  TdeckProKeys k;
  k.key(kAlt, true, 0);
  expectAction(k.key(kS, true, 0), Action::Down);
  expectAction(k.poll(kTdeckProRepeatDelayMs), Action::Down);
  // Letting go of Alt stops it too.
  k.key(kAlt, false, 600);
  expectAction(k.poll(5000), Action::None);
}

void test_mic_key_opens_emoji_only_while_typing() {
  TdeckProKeys k;
  KeyEvent e = k.key(34, true, 0);
  expectAction(e, Action::Emoji);
  TEST_ASSERT_TRUE(e.modifier);  // ignored outside text entry (ui_task)
  k.key(kSym, true, 10);
  expectChar(k.key(34, true, 20), '0');
}

void test_chars_dont_repeat() {
  TdeckProKeys k;
  expectChar(k.key(kQ, true, 0), 'q');
  expectAction(k.poll(5000), Action::None);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_letters_and_space);
  RUN_TEST(test_release_and_out_of_range_ignored);
  RUN_TEST(test_enter_and_backspace);
  RUN_TEST(test_shift_one_shot_either_side);
  RUN_TEST(test_sym_layer_and_symbols_event);
  RUN_TEST(test_sym_chords);
  RUN_TEST(test_modifier_times_out);
  RUN_TEST(test_alt_tap_is_one_shot);
  RUN_TEST(test_alt_held_chords);
  RUN_TEST(test_backspace_repeats_while_held);
  RUN_TEST(test_alt_arrow_repeats_while_held);
  RUN_TEST(test_chars_dont_repeat);
  RUN_TEST(test_mic_key_opens_emoji_only_while_typing);
  return UNITY_END();
}
