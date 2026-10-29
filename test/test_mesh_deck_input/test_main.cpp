#include <unity.h>

#include <cstdint>

#include "input/key_event.h"
#include "input/keymap_mesh_deck.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint8_t L(int r, int c) { return meshDeckKeyPos(0, r, c); }
constexpr uint8_t R(int r, int c) { return meshDeckKeyPos(1, r, c); }
const uint8_t k1 = L(0, 0), k0 = R(0, 4), kQ = L(1, 0), kW = L(1, 1), kA = L(2, 0), kS = L(2, 1),
              kD = L(2, 2), kF = L(2, 3), kShift = L(3, 0), kZ = L(3, 1), kC = L(3, 3), kLag = L(4, 0),
              kTab = L(4, 1), kAlt = L(4, 2), kComma = L(4, 3), kSpace = L(4, 4), kP = R(1, 4),
              kH = R(2, 0), kDel = R(2, 4), kM = R(3, 2), kEnter = R(3, 4), kDot = R(4, 1),
              kMic = R(4, 2), kSym = R(4, 3), kFn = R(4, 4);
}  // namespace

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL((uint32_t)c, (uint32_t)e.ch);
}

static KeyEvent tap(MeshDeckKeys& k, uint8_t pos, uint32_t ms) {
  KeyEvent e = k.key(pos, true, ms);
  k.key(pos, false, ms + 1);
  return e;
}

void test_letters_digits_punctuation() {
  MeshDeckKeys k;
  expectChar(tap(k, kQ, 0), 'q');
  expectChar(tap(k, kP, 0), 'p');
  expectChar(tap(k, kZ, 0), 'z');
  expectChar(tap(k, kM, 0), 'm');
  expectChar(tap(k, k1, 0), '1');
  expectChar(tap(k, k0, 0), '0');
  expectChar(tap(k, kComma, 0), ',');
  expectChar(tap(k, kDot, 0), '.');
  expectChar(tap(k, kSpace, 0), ' ');
}

void test_release_and_unused_keys_ignored() {
  MeshDeckKeys k;
  expectAction(k.key(kQ, false, 0), Action::None);
  expectAction(tap(k, kLag, 0), Action::None);
  expectAction(tap(k, R(3, 3), 0), Action::None);
  expectAction(tap(k, R(4, 0), 0), Action::None);
  expectAction(tap(k, 50, 0), Action::None);
}

void test_special_keys() {
  MeshDeckKeys k;
  expectAction(tap(k, kDel, 0), Action::Backspace);
  expectAction(tap(k, kEnter, 0), Action::Select);
  expectAction(tap(k, kTab, 0), Action::Palette);
  KeyEvent e = tap(k, kMic, 0);
  expectAction(e, Action::Emoji);
  TEST_ASSERT_TRUE(e.modifier);
}

void test_shift_held_and_one_shot() {
  MeshDeckKeys k;
  k.key(kShift, true, 0);
  expectChar(tap(k, kQ, 10), 'Q');
  expectChar(tap(k, kA, 20), 'A');
  k.key(kShift, false, 30);
  expectChar(tap(k, kA, 40), 'a');
  tap(k, kShift, 100);
  expectChar(tap(k, k1, 200), '!');
  expectChar(tap(k, k1, 300), '1');
  tap(k, kShift, 400);
  expectChar(tap(k, kComma, 500), '<');
  tap(k, kShift, 600);
  expectChar(tap(k, kDot, 700), '>');
}

void test_one_shot_times_out() {
  MeshDeckKeys k;
  tap(k, kShift, 0);
  expectChar(tap(k, kQ, kMeshDeckModifierMs + 10), 'q');
}

void test_sym_layer() {
  MeshDeckKeys k;
  tap(k, kSym, 0);
  expectChar(tap(k, kQ, 10), '?');
  k.key(kSym, true, 20);
  expectChar(tap(k, kW, 30), '/');
  expectChar(tap(k, kZ, 40), '<');
  expectChar(tap(k, kComma, 50), '!');
  k.key(kSym, false, 60);
  tap(k, kSym, 70);
  KeyEvent e = tap(k, kEnter, 80);
  expectAction(e, Action::Symbols);
  TEST_ASSERT_TRUE(e.modifier);
}

void test_alt_and_fn_chords() {
  MeshDeckKeys k;
  k.key(kAlt, true, 0);
  expectAction(tap(k, kW, 10), Action::Up);
  expectAction(tap(k, kA, 20), Action::Left);
  expectAction(tap(k, kS, 30), Action::Down);
  expectAction(tap(k, kD, 40), Action::Right);
  expectAction(tap(k, kDel, 50), Action::Back);
  expectAction(tap(k, kEnter, 60), Action::Palette);
  expectAction(tap(k, kQ, 70), Action::None);
  k.key(kAlt, false, 80);
  expectChar(tap(k, kW, 90), 'w');
  k.key(kFn, true, 100);
  expectAction(tap(k, kH, 110), Action::OpenHome);
  expectAction(tap(k, kC, 120), Action::OpenCirc);
  expectAction(tap(k, kM, 130), Action::OpenCmail);
  expectAction(tap(k, kF, 140), Action::OpenFeed);
  k.key(kFn, false, 150);
  tap(k, kAlt, 200);
  expectAction(tap(k, kS, 210), Action::Down);
  expectChar(tap(k, kS, 220), 's');
}

void test_backspace_repeats_while_held() {
  MeshDeckKeys k;
  expectAction(k.key(kDel, true, 0), Action::Backspace);
  expectAction(k.poll(kMeshDeckRepeatDelayMs - 1), Action::None);
  expectAction(k.poll(kMeshDeckRepeatDelayMs), Action::Backspace);
  expectAction(k.poll(kMeshDeckRepeatDelayMs + 1), Action::None);
  expectAction(k.poll(kMeshDeckRepeatDelayMs + kMeshDeckRepeatMs), Action::Backspace);
  k.key(kDel, false, 1000);
  expectAction(k.poll(5000), Action::None);
}

void test_alt_arrow_stops_when_alt_released() {
  MeshDeckKeys k;
  k.key(kAlt, true, 0);
  expectAction(k.key(kS, true, 10), Action::Down);
  expectAction(k.poll(10 + kMeshDeckRepeatDelayMs), Action::Down);
  k.key(kAlt, false, 700);
  expectAction(k.poll(2000), Action::None);
}

void test_chars_dont_repeat() {
  MeshDeckKeys k;
  expectChar(k.key(kQ, true, 0), 'q');
  expectAction(k.poll(5000), Action::None);
}

static int pad(MeshDeckPad& p, uint8_t bits, uint32_t ms, KeyEvent* out) {
  return p.update(bits, ms, out, 4);
}

void test_pad_arrows_on_press_and_repeat() {
  MeshDeckPad p;
  KeyEvent ev[4];
  TEST_ASSERT_EQUAL(1, pad(p, kPadUp, 0, ev));
  expectAction(ev[0], Action::Up);
  TEST_ASSERT_EQUAL(0, pad(p, kPadUp, 100, ev));
  TEST_ASSERT_EQUAL(1, pad(p, kPadUp, kMeshDeckRepeatDelayMs, ev));
  expectAction(ev[0], Action::Up);
  TEST_ASSERT_EQUAL(1, pad(p, kPadUp, kMeshDeckRepeatDelayMs + kMeshDeckRepeatMs, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 1000, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 1010, ev));
  TEST_ASSERT_EQUAL(1, pad(p, kPadLeft, 1020, ev));
  expectAction(ev[0], Action::Left);
  TEST_ASSERT_EQUAL(0, pad(p, 0, 1030, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 1040, ev));
  TEST_ASSERT_EQUAL(2, pad(p, kPadRight | kPadDown, 1050, ev));
  expectAction(ev[0], Action::Down);
  expectAction(ev[1], Action::Right);
}

void test_pad_release_bounce_is_not_a_second_press() {
  MeshDeckPad p;
  KeyEvent ev[4];
  TEST_ASSERT_EQUAL(1, pad(p, kPadDown, 0, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 10, ev));         // one released read: a bounce
  TEST_ASSERT_EQUAL(0, pad(p, kPadDown, 20, ev));  // still the same press
  TEST_ASSERT_EQUAL(0, pad(p, 0, 30, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 40, ev));         // released now
  TEST_ASSERT_EQUAL(1, pad(p, kPadDown, 50, ev));
}

void test_pad_select_tap_and_hold() {
  MeshDeckPad p;
  KeyEvent ev[4];
  TEST_ASSERT_EQUAL(0, pad(p, kPadSelect, 0, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 100, ev));
  TEST_ASSERT_EQUAL(1, pad(p, 0, 110, ev));
  expectAction(ev[0], Action::Select);
  TEST_ASSERT_EQUAL(0, pad(p, kPadSelect, 200, ev));
  TEST_ASSERT_EQUAL(1, pad(p, kPadSelect, 200 + kMeshDeckHoldMs, ev));
  expectAction(ev[0], Action::ScreenOff);
  TEST_ASSERT_EQUAL(0, pad(p, kPadSelect, 5000, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 5010, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 5020, ev));
}

void test_pad_l1_back_hold_home() {
  MeshDeckPad p;
  KeyEvent ev[4];
  TEST_ASSERT_EQUAL(0, pad(p, kPadL1, 0, ev));
  TEST_ASSERT_EQUAL(0, pad(p, 0, 50, ev));
  TEST_ASSERT_EQUAL(1, pad(p, 0, 60, ev));
  expectAction(ev[0], Action::Back);
  TEST_ASSERT_EQUAL(0, pad(p, kPadL1, 100, ev));
  TEST_ASSERT_EQUAL(1, pad(p, kPadL1, 100 + kMeshDeckHoldMs, ev));
  expectAction(ev[0], Action::OpenHome);
}

void test_pad_screen_buttons() {
  MeshDeckPad p;
  KeyEvent ev[4];
  TEST_ASSERT_EQUAL(1, pad(p, kPadR2, 0, ev));
  expectAction(ev[0], Action::ScreenOff);
  TEST_ASSERT_EQUAL(0, pad(p, kPadR2, 3000, ev));
  pad(p, 0, 3010, ev);
  pad(p, 0, 3020, ev);
  TEST_ASSERT_EQUAL(1, pad(p, kPadBoot, 3030, ev));
  expectAction(ev[0], Action::ScreenOff);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_letters_digits_punctuation);
  RUN_TEST(test_release_and_unused_keys_ignored);
  RUN_TEST(test_special_keys);
  RUN_TEST(test_shift_held_and_one_shot);
  RUN_TEST(test_one_shot_times_out);
  RUN_TEST(test_sym_layer);
  RUN_TEST(test_alt_and_fn_chords);
  RUN_TEST(test_backspace_repeats_while_held);
  RUN_TEST(test_alt_arrow_stops_when_alt_released);
  RUN_TEST(test_chars_dont_repeat);
  RUN_TEST(test_pad_arrows_on_press_and_repeat);
  RUN_TEST(test_pad_release_bounce_is_not_a_second_press);
  RUN_TEST(test_pad_select_tap_and_hold);
  RUN_TEST(test_pad_l1_back_hold_home);
  RUN_TEST(test_pad_screen_buttons);
  return UNITY_END();
}
