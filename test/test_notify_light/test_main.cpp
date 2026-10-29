#include <unity.h>

#include <string_view>
#include <utility>
#include <vector>

#include "ui/notify_light.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
using Changes = std::vector<std::pair<uint32_t, uint8_t>>;

// Ticks every millisecond over [from, to) and records each change of output,
// starting from `last`.
Changes run(NotifyBlinker& b, uint32_t from, uint32_t to, LedColor c, uint16_t timeout,
            bool unread, uint8_t& last) {
  Changes out;
  for (uint32_t t = from; t != to; ++t) {
    const uint8_t v = b.tick(t, c, timeout, unread);
    if (v != last) out.push_back({t, v});
    last = v;
  }
  return out;
}

void expectDoubleBlink(const Changes& ch, size_t at, uint32_t start, uint8_t rgb) {
  TEST_ASSERT_TRUE(ch.size() >= at + 4);
  const uint32_t times[] = {start, start + 60, start + 150, start + 210};
  const uint8_t vals[] = {rgb, 0, rgb, 0};
  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_EQUAL_UINT32(times[i], ch[at + i].first);
    TEST_ASSERT_EQUAL_UINT8(vals[i], ch[at + i].second);
  }
}
}  // namespace

void test_rgb_mapping() {
  TEST_ASSERT_EQUAL_UINT8(0, ledColorRgb(LedColor::Off));
  TEST_ASSERT_EQUAL_UINT8(1, ledColorRgb(LedColor::Red));
  TEST_ASSERT_EQUAL_UINT8(2, ledColorRgb(LedColor::Green));
  TEST_ASSERT_EQUAL_UINT8(4, ledColorRgb(LedColor::Blue));
  TEST_ASSERT_EQUAL_UINT8(3, ledColorRgb(LedColor::Yellow));
  TEST_ASSERT_EQUAL_UINT8(6, ledColorRgb(LedColor::Cyan));
  TEST_ASSERT_EQUAL_UINT8(5, ledColorRgb(LedColor::Magenta));
  TEST_ASSERT_EQUAL_UINT8(7, ledColorRgb(LedColor::White));
}

void test_keys_labels_and_parse() {
  for (int i = 0; i < kLedColorCount; ++i) {
    const LedColor c = ledColorFromIndex(i);
    LedColor back = LedColor::Off;
    TEST_ASSERT_TRUE(parseLedColor(ledColorKey(c), back));
    TEST_ASSERT(back == c);
  }
  TEST_ASSERT_EQUAL_STRING("magenta", ledColorKey(LedColor::Magenta));
  TEST_ASSERT_TRUE(std::u32string_view(ledColorLabel(LedColor::Cyan)) == U"Cyan");
  LedColor c = LedColor::Green;
  TEST_ASSERT_FALSE(parseLedColor("purple", c));
  TEST_ASSERT(c == LedColor::Green);
  TEST_ASSERT(ledColorFromIndex(8, LedColor::Blue) == LedColor::Blue);
  TEST_ASSERT(ledColorFromIndex(-1) == LedColor::Off);
}

void test_timeout_options() {
  TEST_ASSERT_TRUE(validNotifyTimeout(300));
  TEST_ASSERT_TRUE(validNotifyTimeout(0));
  TEST_ASSERT_FALSE(validNotifyTimeout(45));
  TEST_ASSERT_EQUAL_UINT16(60, snapNotifyTimeout(45));     // up to the next option
  TEST_ASSERT_EQUAL_UINT16(15, snapNotifyTimeout(1));
  TEST_ASSERT_EQUAL_UINT16(300, snapNotifyTimeout(9999));  // past the longest
  TEST_ASSERT_EQUAL_UINT16(0, snapNotifyTimeout(0));
  TEST_ASSERT_TRUE(std::u32string_view(notifyTimeoutLabel(60)) == U"1 min");
  TEST_ASSERT_TRUE(std::u32string_view(notifyTimeoutLabel(0)) == U"Until read");
  TEST_ASSERT_TRUE(std::u32string_view(notifyTimeoutLabel(20)) == U"30 s");
}

void test_double_blink_then_repeat_each_second() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(1000);
  Changes ch = run(b, 1000, 3500, LedColor::Blue, 15, true, last);
  TEST_ASSERT_EQUAL(12, int(ch.size()));
  expectDoubleBlink(ch, 0, 1000, 4);
  expectDoubleBlink(ch, 4, 2000, 4);
  expectDoubleBlink(ch, 8, 3000, 4);
}

void test_stops_when_read_and_finishes_dark() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(0);
  run(b, 0, 20, LedColor::Red, 15, true, last);
  TEST_ASSERT_EQUAL_UINT8(1, last);  // mid-pattern, lit
  Changes ch = run(b, 20, 5000, LedColor::Red, 15, false, last);
  // The pattern under way finishes, dark, and none follows.
  TEST_ASSERT_EQUAL(3, int(ch.size()));
  TEST_ASSERT_EQUAL_UINT32(210, ch[2].first);
  TEST_ASSERT_EQUAL_UINT8(0, last);
  TEST_ASSERT_FALSE(b.active());
}

void test_stops_after_timeout() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(1000);
  Changes ch = run(b, 1000, 20000, LedColor::Green, 15, true, last);
  // Patterns start at 1000 .. 15000; 16000 is 15 s after the message.
  TEST_ASSERT_EQUAL(15 * 4, int(ch.size()));
  TEST_ASSERT_EQUAL_UINT32(15000 + 210, ch.back().first);
  TEST_ASSERT_EQUAL_UINT8(0, last);
  TEST_ASSERT_FALSE(b.active());
  // A new message arms it again.
  b.message(30000);
  ch = run(b, 30000, 30300, LedColor::Green, 15, true, last);
  expectDoubleBlink(ch, 0, 30000, 2);
}

void test_timeout_zero_runs_until_read() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(0);
  Changes ch = run(b, 0, 120000, LedColor::White, 0, true, last);
  TEST_ASSERT_EQUAL(120 * 4, int(ch.size()));
  ch = run(b, 120000, 125000, LedColor::White, 0, false, last);
  TEST_ASSERT_EQUAL(0, int(ch.size()));
  TEST_ASSERT_FALSE(b.active());
}

void test_preview_once_without_unread() {
  NotifyBlinker b;
  uint8_t last = 0;
  TEST_ASSERT_FALSE(b.active());
  b.preview(LedColor::Magenta, 500);
  TEST_ASSERT_TRUE(b.active());
  // The saved colour is Off: the preview shows its own anyway.
  Changes ch = run(b, 500, 5000, LedColor::Off, 15, false, last);
  TEST_ASSERT_EQUAL(4, int(ch.size()));
  expectDoubleBlink(ch, 0, 500, 5);
  TEST_ASSERT_FALSE(b.active());
}

void test_off_disables() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(0);
  Changes ch = run(b, 0, 3000, LedColor::Off, 15, true, last);
  TEST_ASSERT_EQUAL(0, int(ch.size()));
  TEST_ASSERT_FALSE(b.active());
  b.preview(LedColor::Off, 4000);
  TEST_ASSERT_FALSE(b.active());
}

void test_cancel_goes_dark() {
  NotifyBlinker b;
  b.message(0);
  TEST_ASSERT_EQUAL_UINT8(4, b.tick(0, LedColor::Blue, 15, true));
  b.cancel();
  TEST_ASSERT_EQUAL_UINT8(0, b.tick(1, LedColor::Blue, 15, true));
  TEST_ASSERT_FALSE(b.active());
}

void test_millis_wraparound() {
  NotifyBlinker b;
  uint8_t last = 0;
  const uint32_t t0 = 0xFFFFFF00u;
  b.message(t0);
  Changes ch = run(b, t0, t0 + 2500, LedColor::Cyan, 15, true, last);
  TEST_ASSERT_EQUAL(12, int(ch.size()));
  expectDoubleBlink(ch, 0, t0, 6);
  expectDoubleBlink(ch, 4, t0 + 1000, 6);
  expectDoubleBlink(ch, 8, t0 + 2000, 6);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rgb_mapping);
  RUN_TEST(test_keys_labels_and_parse);
  RUN_TEST(test_timeout_options);
  RUN_TEST(test_double_blink_then_repeat_each_second);
  RUN_TEST(test_stops_when_read_and_finishes_dark);
  RUN_TEST(test_stops_after_timeout);
  RUN_TEST(test_timeout_zero_runs_until_read);
  RUN_TEST(test_preview_once_without_unread);
  RUN_TEST(test_off_disables);
  RUN_TEST(test_cancel_goes_dark);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
