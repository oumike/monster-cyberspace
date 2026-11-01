#include <unity.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "input/key_event.h"
#include "input/keymap_cardputer_adv.h"

using namespace rc;

void setUp() {}
void tearDown() {}

// TCA8418 key numbers (1-based FIFO codes) for the keys the tests press.
constexpr uint8_t kBacktick = 1, kCtrl = 4, kFn = 3, kShift = 7, kOpt = 8, kQ = 6, kA = 13,
                  kAlt = 14, kComma = 54, kSemicolon = 57, kPeriod = 58, kSlash = 64,
                  kBackspace = 65, kEnter = 67, kSpace = 68, kOne = 5, kP = 52;

static void expectAction(const KeyEvent& e, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(e.action));
}

static void expectChar(const KeyEvent& e, char c) {
  expectAction(e, Action::Char);
  TEST_ASSERT_EQUAL((uint32_t)c, (uint32_t)e.ch);
}

void test_letters_and_shift() {
  CardputerKeys k;
  expectChar(k.key(kA, true, 0), 'a');
  expectChar(k.key(kQ, true, 0), 'q');
  expectChar(k.key(kP, true, 0), 'p');
  expectChar(k.key(kOne, true, 0), '1');
  expectChar(k.key(kSpace, true, 0), ' ');
  expectChar(k.key(kSemicolon, true, 0), ';');
  expectAction(k.key(kShift, true, 0), Action::None);
  expectChar(k.key(kA, true, 10), 'A');
  expectChar(k.key(kOne, true, 20), '!');
  expectChar(k.key(kSlash, true, 30), '?');
  expectAction(k.key(kShift, false, 40), Action::None);
  expectChar(k.key(kA, true, 50), 'a');
}

void test_fn_arrows() {
  CardputerKeys k;
  k.key(kFn, true, 0);
  expectAction(k.key(kSemicolon, true, 10), Action::Up);
  expectAction(k.key(kPeriod, true, 20), Action::Down);
  expectAction(k.key(kComma, true, 30), Action::Left);
  expectAction(k.key(kSlash, true, 40), Action::Right);
  k.key(kFn, false, 50);
  expectChar(k.key(kSemicolon, true, 60), ';');
}

void test_esc_is_back() {
  CardputerKeys k;
  k.setTextActive(false);
  expectAction(k.key(kBacktick, true, 0), Action::Back);
}

void test_enter_selects() {
  CardputerKeys k;
  expectAction(k.key(kEnter, true, 0), Action::Select);
}

void test_backspace_repeats() {
  CardputerKeys k;
  expectAction(k.key(kBackspace, true, 0), Action::Backspace);
  expectAction(k.poll(kCardputerRepeatDelayMs - 1), Action::None);
  expectAction(k.poll(kCardputerRepeatDelayMs), Action::Backspace);
  expectAction(k.poll(kCardputerRepeatDelayMs + kCardputerRepeatMs - 1), Action::None);
  expectAction(k.poll(kCardputerRepeatDelayMs + kCardputerRepeatMs), Action::Backspace);
  expectAction(k.key(kBackspace, false, 1000), Action::None);
  expectAction(k.poll(10000), Action::None);
}

void test_fn_backspace_is_home() {
  CardputerKeys k;
  k.key(kFn, true, 0);
  expectAction(k.key(kBackspace, true, 10), Action::OpenHome);
  expectAction(k.poll(5000), Action::None);
}

void test_modifiers_alone_send_nothing() {
  CardputerKeys k;
  for (uint8_t m : {kFn, kShift, kCtrl, kOpt, kAlt}) {
    expectAction(k.key(m, true, 0), Action::None);
    expectAction(k.key(m, false, 10), Action::None);
  }
  expectChar(k.key(kA, true, 20), 'a');
}

void test_release_sends_nothing() {
  CardputerKeys k;
  expectAction(k.key(kA, false, 0), Action::None);
  expectAction(k.key(kEnter, false, 0), Action::None);
  expectAction(k.key(kBacktick, false, 0), Action::None);
  expectAction(k.key(0, true, 0), Action::None);
  expectAction(k.key(9, true, 0), Action::None);   // column 8: not wired
  expectAction(k.key(71, true, 0), Action::None);  // past the 7x8 matrix
}

// Every wired key, by TCA8418 FIFO number, worked out by hand from M5Cardputer
// 1.1.1: TCA8418.cpp get_key_event_raw (n - 1 = 10 * row + col) and remap
// (layout column 2 * row, +1 for cols 4-7; layout row col % 4) into Keyboard.h's
// _key_value_map. Char rows give normal and Shift output; other rows give the
// action for both layers (0 = Action::None), in a text field.
struct KeyRow {
  uint8_t num;
  char normal, shifted;
  Action action;
};

constexpr KeyRow kAllKeys[] = {
    {1, 0, 0, Action::Up},         {2, 0, 0, Action::None},  // ` (Esc: leaves the field), Tab
    {3, 0, 0, Action::None},       {4, 0, 0, Action::None},  // Fn, Ctrl
    {5, '1', '!', Action::Char},   {6, 'q', 'Q', Action::Char},
    {7, 0, 0, Action::None},       {8, 0, 0, Action::None},  // Shift, Opt
    {11, '2', '@', Action::Char},  {12, 'w', 'W', Action::Char},
    {13, 'a', 'A', Action::Char},  {14, 0, 0, Action::None},  // Alt
    {15, '3', '#', Action::Char},  {16, 'e', 'E', Action::Char},
    {17, 's', 'S', Action::Char},  {18, 'z', 'Z', Action::Char},
    {21, '4', '$', Action::Char},  {22, 'r', 'R', Action::Char},
    {23, 'd', 'D', Action::Char},  {24, 'x', 'X', Action::Char},
    {25, '5', '%', Action::Char},  {26, 't', 'T', Action::Char},
    {27, 'f', 'F', Action::Char},  {28, 'c', 'C', Action::Char},
    {31, '6', '^', Action::Char},  {32, 'y', 'Y', Action::Char},
    {33, 'g', 'G', Action::Char},  {34, 'v', 'V', Action::Char},
    {35, '7', '&', Action::Char},  {36, 'u', 'U', Action::Char},
    {37, 'h', 'H', Action::Char},  {38, 'b', 'B', Action::Char},
    {41, '8', '*', Action::Char},  {42, 'i', 'I', Action::Char},
    {43, 'j', 'J', Action::Char},  {44, 'n', 'N', Action::Char},
    {45, '9', '(', Action::Char},  {46, 'o', 'O', Action::Char},
    {47, 'k', 'K', Action::Char},  {48, 'm', 'M', Action::Char},
    {51, '0', ')', Action::Char},  {52, 'p', 'P', Action::Char},
    {53, 'l', 'L', Action::Char},  {54, ',', '<', Action::Char},
    {55, '-', '_', Action::Char},  {56, '[', '{', Action::Char},
    {57, ';', ':', Action::Char},  {58, '.', '>', Action::Char},
    {61, '=', '+', Action::Char},  {62, ']', '}', Action::Char},
    {63, '\'', '"', Action::Char}, {64, '/', '?', Action::Char},
    {65, 0, 0, Action::Backspace}, {66, '\\', '|', Action::Char},
    {67, 0, 0, Action::Select},    {68, ' ', ' ', Action::Char},
};

static void expectRow(const KeyRow& r, const KeyEvent& e, bool shift) {
  char msg[32];
  snprintf(msg, sizeof msg, "key %u%s", r.num, shift ? " shifted" : "");
  TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(r.action), static_cast<int>(e.action), msg);
  if (r.action == Action::Char)
    TEST_ASSERT_EQUAL_MESSAGE((uint32_t)(shift ? r.shifted : r.normal), (uint32_t)e.ch, msg);
}

void test_whole_key_table() {
  TEST_ASSERT_EQUAL(56, sizeof kAllKeys / sizeof kAllKeys[0]);
  for (bool shift : {false, true}) {
    CardputerKeys k;
    if (shift) k.key(kShift, true, 0);
    for (const KeyRow& r : kAllKeys) {
      if (r.num == kShift || r.num == kFn) continue;  // would change the layer
      expectRow(r, k.key(r.num, true, 10), shift);
      expectAction(k.key(r.num, false, 20), Action::None);
    }
  }
}

void test_unwired_numbers_send_nothing() {
  CardputerKeys k;
  for (unsigned n = 0; n < 256; ++n) {
    bool wired = false;
    for (const KeyRow& r : kAllKeys) wired |= r.num == n;
    if (wired) continue;
    expectAction(k.key(static_cast<uint8_t>(n), true, 0), Action::None);
    expectAction(k.key(static_cast<uint8_t>(n), false, 0), Action::None);
  }
}

void test_tab_sends_nothing() {
  CardputerKeys k;
  constexpr uint8_t kTab = 2;
  expectAction(k.key(kTab, true, 0), Action::None);
  k.key(kShift, true, 10);
  expectAction(k.key(kTab, true, 20), Action::None);
  k.key(kShift, false, 30);
  k.key(kFn, true, 40);
  expectAction(k.key(kTab, true, 50), Action::None);
}

void test_no_text_field_plain_keys_navigate() {
  CardputerKeys k;
  k.setTextActive(false);
  expectAction(k.key(kSemicolon, true, 0), Action::Up);
  expectAction(k.key(kPeriod, true, 10), Action::Down);
  expectAction(k.key(kComma, true, 20), Action::Left);
  expectAction(k.key(kSlash, true, 30), Action::Right);
  k.key(kShift, true, 40);
  expectAction(k.key(kComma, true, 50), Action::Left);  // the < key
  k.key(kShift, false, 60);
  expectAction(k.key(kBacktick, true, 70), Action::Back);
  expectChar(k.key(kA, true, 80), 'a');
}

void test_esc_leaves_text_field_then_typing_returns() {
  CardputerKeys k;
  k.setTextActive(true);
  expectChar(k.key(kSemicolon, true, 0), ';');
  // Esc moves to the conversation: Up selects the newest message.
  expectAction(k.key(kBacktick, true, 10), Action::Up);
  expectAction(k.key(kSemicolon, true, 20), Action::Up);
  expectAction(k.key(kPeriod, true, 30), Action::Down);
  expectAction(k.key(kBacktick, true, 40), Action::Back);
  // A letter goes back to the text field and is typed.
  expectChar(k.key(kA, true, 50), 'a');
  expectChar(k.key(kPeriod, true, 60), '.');
}

void test_new_text_field_starts_typing() {
  CardputerKeys k;
  k.setTextActive(true);
  k.key(kBacktick, true, 0);
  k.setTextActive(false);
  k.setTextActive(true);
  expectChar(k.key(kSemicolon, true, 10), ';');
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_letters_and_shift);
  RUN_TEST(test_fn_arrows);
  RUN_TEST(test_esc_is_back);
  RUN_TEST(test_enter_selects);
  RUN_TEST(test_backspace_repeats);
  RUN_TEST(test_fn_backspace_is_home);
  RUN_TEST(test_modifiers_alone_send_nothing);
  RUN_TEST(test_release_sends_nothing);
  RUN_TEST(test_whole_key_table);
  RUN_TEST(test_unwired_numbers_send_nothing);
  RUN_TEST(test_tab_sends_nothing);
  RUN_TEST(test_no_text_field_plain_keys_navigate);
  RUN_TEST(test_esc_leaves_text_field_then_typing_returns);
  RUN_TEST(test_new_text_field_starts_typing);
  return UNITY_END();
}
