#include <unity.h>

#include <cstdint>
#include <initializer_list>

#include "input/backspace.h"
#include "input/key_event.h"
#include "input/keymap_tdeck.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectChar(uint8_t raw) {
  KeyEvent e = mapTdeckKey(raw);
  TEST_ASSERT_EQUAL(static_cast<int>(Action::Char), static_cast<int>(e.action));
  TEST_ASSERT_EQUAL((uint32_t)raw, (uint32_t)e.ch);
}

static void expectAction(uint8_t raw, Action a) {
  TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(mapTdeckKey(raw).action));
}

void test_tdeck_printable() {
  expectChar('a');
  expectChar(' ');
  expectChar('~');
  expectChar('Q');
}

void test_tdeck_controls() {
  expectAction(0x08, Action::Backspace);
  expectAction(0x0D, Action::Select);
  expectAction(0x0A, Action::Select);
}

void test_tdeck_unknown() {
  for (uint8_t raw : {0x00, 0x1B, 0x7F, 0xB5}) expectAction(raw, Action::None);
}

void test_backspace_consumed() {
  TEST_ASSERT_EQUAL(static_cast<int>(BackspaceUse::Delete),
                    static_cast<int>(resolveBackspace(true, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(BackspaceUse::Delete),
                    static_cast<int>(resolveBackspace(true, UINT32_MAX)));
}

void test_backspace_back() {
  TEST_ASSERT_EQUAL(static_cast<int>(BackspaceUse::Back),
                    static_cast<int>(resolveBackspace(false, UINT32_MAX)));
}

void test_backspace_guard_399() {
  TEST_ASSERT_EQUAL(static_cast<int>(BackspaceUse::Drop),
                    static_cast<int>(resolveBackspace(false, 399)));
}

void test_backspace_guard_400() {
  TEST_ASSERT_EQUAL(static_cast<int>(BackspaceUse::Back),
                    static_cast<int>(resolveBackspace(false, 400)));
}

void test_any_input_wakes() {
  KeyEvent c;
  c.action = Action::Char;
  c.ch = 'a';
  TEST_ASSERT_TRUE(wakesScreen(c, true));
  for (Action a : {Action::Select, Action::Up, Action::Backspace, Action::ScreenOff}) {
    KeyEvent e;
    e.action = a;
    TEST_ASSERT_TRUE(wakesScreen(e, true));
  }
  TEST_ASSERT_FALSE(wakesScreen(KeyEvent{}, true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tdeck_printable);
  RUN_TEST(test_tdeck_controls);
  RUN_TEST(test_tdeck_unknown);
  RUN_TEST(test_backspace_consumed);
  RUN_TEST(test_backspace_back);
  RUN_TEST(test_backspace_guard_399);
  RUN_TEST(test_backspace_guard_400);
  RUN_TEST(test_any_input_wakes);
  return UNITY_END();
}
