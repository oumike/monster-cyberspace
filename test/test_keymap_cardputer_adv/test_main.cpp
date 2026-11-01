#include <unity.h>

#include <cstdint>
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
  return UNITY_END();
}
