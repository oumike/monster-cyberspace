#include <unity.h>

#include <string>

#include "input/keymap_m9.h"
#include "web/web_util.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expect(uint8_t raw, Action a, bool early) {
  KeyEvent e = mapM9Key(raw, early);
  char msg[32];
  snprintf(msg, sizeof msg, "raw 0x%02X early %d", raw, early);
  TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(a), static_cast<int>(e.action), msg);
}

static void expectBoth(uint8_t raw, Action a) {
  expect(raw, a, false);
  expect(raw, a, true);
}

void test_keymap_table() {
  expectBoth(0x08, Action::Backspace);
  expectBoth(0x7F, Action::Backspace);
  expectBoth(0x0D, Action::Select);
  expectBoth(0x0A, Action::Select);
  expectBoth(0x81, Action::OpenCirc);
  expectBoth(0x82, Action::OpenHome);
  expectBoth(0x83, Action::OpenCmail);
  expectBoth(0x84, Action::OpenFeed);
  expectBoth(0x85, Action::OpenMap);
  expect(0x86, Action::Back, true);
  expect(0x86, Action::None, false);
  expect(0x87, Action::Back, false);
  expect(0x87, Action::None, true);
  expectBoth(0x88, Action::Palette);
  expectBoth(0x90, Action::Palette);
  expectBoth(0x89, Action::ClearLine);
  expectBoth(0xA3, Action::ScreenOff);
  expectBoth(0xB4, Action::Left);
  expectBoth(0xB5, Action::Up);
  expectBoth(0xB6, Action::Down);
  expectBoth(0xB7, Action::Right);
  expectBoth(0x00, Action::None);
  expectBoth(0x1B, Action::None);
  expectBoth(0x80, Action::None);
  expectBoth(0xFF, Action::None);
}

void test_keymap_chars() {
  for (int c = 0x20; c <= 0x7E; ++c) {
    KeyEvent e = mapM9Key(static_cast<uint8_t>(c), false);
    TEST_ASSERT_EQUAL(static_cast<int>(Action::Char), static_cast<int>(e.action));
    TEST_ASSERT_EQUAL_HEX32(c, e.ch);
  }
  TEST_ASSERT_EQUAL_HEX32(0, mapM9Key(0x0D, false).ch);
}

void test_wakes_screen() {
  TEST_ASSERT_TRUE(wakesScreen(mapM9Key(0xA3, false)));
  TEST_ASSERT_FALSE(wakesScreen(mapM9Key(0x0D, false)));
  TEST_ASSERT_FALSE(wakesScreen(mapM9Key('a', false)));
  TEST_ASSERT_FALSE(wakesScreen(mapM9Key(0xB5, false)));
  TEST_ASSERT_FALSE(wakesScreen(mapM9Key(0x87, false)));
  TEST_ASSERT_FALSE(wakesScreen(KeyEvent{}));
  TEST_ASSERT_TRUE(peeksScreen(mapM9Key(0x0D, false)));
  TEST_ASSERT_FALSE(peeksScreen(mapM9Key(0xA3, false)));
  TEST_ASSERT_FALSE(peeksScreen(mapM9Key('a', false)));
}

void test_ap_ssid() {
  TEST_ASSERT_EQUAL_STRING("Rhino-Cyberspace-M9-Setup", setupApSsid("M9").c_str());
}

void test_ap_password() {
  TEST_ASSERT_TRUE(validApPassword(""));
  TEST_ASSERT_FALSE(validApPassword("short"));
  TEST_ASSERT_TRUE(validApPassword("12345678"));
  TEST_ASSERT_TRUE(validApPassword(std::string(63, 'x')));
  TEST_ASSERT_FALSE(validApPassword(std::string(64, 'x')));
  TEST_ASSERT_FALSE(validApPassword("1234567\x01"));
  TEST_ASSERT_FALSE(validApPassword("caf\xc3\xa9" "1234"));
}

void test_basic_auth() {
  TEST_ASSERT_TRUE(checkBasicAuth("Basic YWRtaW46cGFzcw==", "pass"));
  TEST_ASSERT_FALSE(checkBasicAuth("Basic YWRtaW46cGFzcw==", "nope"));
  TEST_ASSERT_TRUE(checkBasicAuth("", ""));
  TEST_ASSERT_TRUE(checkBasicAuth("Basic garbage", ""));
  TEST_ASSERT_FALSE(checkBasicAuth("", "pass"));
  TEST_ASSERT_FALSE(checkBasicAuth("Bearer YWRtaW46cGFzcw==", "pass"));
  TEST_ASSERT_FALSE(checkBasicAuth("Basic @@@", "pass"));
  // "root:pass" — wrong user.
  TEST_ASSERT_FALSE(checkBasicAuth("Basic cm9vdDpwYXNz", "pass"));
}

void test_url_decode() {
  TEST_ASSERT_EQUAL_STRING("My Net", urlDecode("My%20Net").c_str());
  TEST_ASSERT_EQUAL_STRING("a b", urlDecode("a+b").c_str());
  TEST_ASSERT_EQUAL_STRING("caf\xc3\xa9", urlDecode("caf%C3%A9").c_str());
  // Malformed escapes pass through unchanged.
  TEST_ASSERT_EQUAL_STRING("100%", urlDecode("100%").c_str());
  TEST_ASSERT_EQUAL_STRING("%zz", urlDecode("%zz").c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_keymap_table);
  RUN_TEST(test_keymap_chars);
  RUN_TEST(test_wakes_screen);
  RUN_TEST(test_ap_ssid);
  RUN_TEST(test_ap_password);
  RUN_TEST(test_basic_auth);
  RUN_TEST(test_url_decode);
  return UNITY_END();
}
