#include <unity.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "input/backspace.h"
#include "input/click_timer.h"
#include "input/key_event.h"
#include "input/keymap_tdeck.h"
#include "power/battery.h"

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

static void expectPoll(ClickTimer& t, uint32_t ms, Action a) {
  char msg[24];
  snprintf(msg, sizeof msg, "poll %u", (unsigned)ms);
  TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(a), static_cast<int>(t.poll(ms)), msg);
}

void test_click_tap() {
  ClickTimer t;
  t.press(0);
  t.release(100);
  expectPoll(t, 100, Action::Select);
  expectPoll(t, 120, Action::None);
}

void test_click_hold_screen_off() {
  ClickTimer t;
  t.press(0);
  expectPoll(t, 1999, Action::None);
  expectPoll(t, 2000, Action::ScreenOff);
  expectPoll(t, 2500, Action::None);
}

void test_hold_no_select_on_release() {
  ClickTimer t;
  t.press(0);
  expectPoll(t, 2000, Action::ScreenOff);
  t.release(3000);
  expectPoll(t, 3000, Action::None);
  expectPoll(t, 4000, Action::None);
}

void test_click_during_motion_deferred() {
  ClickTimer t;
  t.motion(0);
  t.press(50);
  t.release(100);
  expectPoll(t, 100, Action::None);
  expectPoll(t, 159, Action::None);
  expectPoll(t, 160, Action::Select);
}

void test_click_during_motion_extends() {
  ClickTimer t;
  t.motion(0);
  t.press(50);
  t.release(100);
  t.motion(150);
  expectPoll(t, 200, Action::None);
  expectPoll(t, 310, Action::Select);
}

void test_motion_after_click_irrelevant() {
  ClickTimer t;
  t.press(0);
  t.release(50);
  expectPoll(t, 50, Action::Select);
  t.motion(60);
  expectPoll(t, 60, Action::None);
  expectPoll(t, 300, Action::None);
}

void test_battery_percent() {
  TEST_ASSERT_EQUAL(-1, batteryPercentFromVolts(2.49f));
  TEST_ASSERT_EQUAL(0, batteryPercentFromVolts(3.0f));
  TEST_ASSERT_EQUAL(0, batteryPercentFromVolts(3.3f));
  TEST_ASSERT_EQUAL(50, batteryPercentFromVolts(3.725f));
  TEST_ASSERT_EQUAL(100, batteryPercentFromVolts(4.15f));
  TEST_ASSERT_EQUAL(100, batteryPercentFromVolts(4.4f));
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
  RUN_TEST(test_click_tap);
  RUN_TEST(test_click_hold_screen_off);
  RUN_TEST(test_hold_no_select_on_release);
  RUN_TEST(test_click_during_motion_deferred);
  RUN_TEST(test_click_during_motion_extends);
  RUN_TEST(test_motion_after_click_irrelevant);
  RUN_TEST(test_battery_percent);
  return UNITY_END();
}
