#include <unity.h>

#include <string_view>
#include <utility>
#include <vector>

#include "ui/led_level.h"
#include "ui/notify_light.h"
#include "ui/wifi_light.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
using Changes = std::vector<std::pair<uint32_t, uint8_t>>;

NotifyInputs only(NotifySource src, LedColor c, bool unread) {
  NotifyInputs in;
  in.color[int(src)] = c;
  in.unread[int(src)] = unread;
  return in;
}

// Ticks every millisecond over [from, to) and records each change of output,
// starting from `last`.
Changes run(NotifyBlinker& b, uint32_t from, uint32_t to, const NotifyInputs& in, uint16_t timeout,
            uint8_t& last) {
  Changes out;
  for (uint32_t t = from; t != to; ++t) {
    const uint8_t v = b.tick(t, in, timeout);
    if (v != last) out.push_back({t, v});
    last = v;
  }
  return out;
}

// One source, the channel.
Changes run(NotifyBlinker& b, uint32_t from, uint32_t to, LedColor c, uint16_t timeout,
            bool unread, uint8_t& last) {
  return run(b, from, to, only(NotifySource::Channel, c, unread), timeout, last);
}

constexpr NotifySource kCh = NotifySource::Channel;

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
  b.message(kCh, 1000);
  Changes ch = run(b, 1000, 3500, LedColor::Blue, 15, true, last);
  TEST_ASSERT_EQUAL(12, int(ch.size()));
  expectDoubleBlink(ch, 0, 1000, 4);
  expectDoubleBlink(ch, 4, 2000, 4);
  expectDoubleBlink(ch, 8, 3000, 4);
}

void test_stops_when_read_and_finishes_dark() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(kCh, 0);
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
  b.message(kCh, 1000);
  Changes ch = run(b, 1000, 20000, LedColor::Green, 15, true, last);
  // Patterns start at 1000 .. 15000; 16000 is 15 s after the message.
  TEST_ASSERT_EQUAL(15 * 4, int(ch.size()));
  TEST_ASSERT_EQUAL_UINT32(15000 + 210, ch.back().first);
  TEST_ASSERT_EQUAL_UINT8(0, last);
  TEST_ASSERT_FALSE(b.active());
  // A new message arms it again.
  b.message(kCh, 30000);
  ch = run(b, 30000, 30300, LedColor::Green, 15, true, last);
  expectDoubleBlink(ch, 0, 30000, 2);
}

void test_timeout_zero_runs_until_read() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(kCh, 0);
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
  b.message(kCh, 0);
  Changes ch = run(b, 0, 3000, LedColor::Off, 15, true, last);
  TEST_ASSERT_EQUAL(0, int(ch.size()));
  TEST_ASSERT_FALSE(b.active());
  b.preview(LedColor::Off, 4000);
  TEST_ASSERT_FALSE(b.active());
}

void test_cancel_goes_dark() {
  NotifyBlinker b;
  b.message(kCh, 0);
  TEST_ASSERT_EQUAL_UINT8(4, b.tick(0, only(kCh, LedColor::Blue, true), 15));
  b.cancel();
  TEST_ASSERT_EQUAL_UINT8(0, b.tick(1, only(kCh, LedColor::Blue, true), 15));
  TEST_ASSERT_FALSE(b.active());
}

void test_millis_wraparound() {
  NotifyBlinker b;
  uint8_t last = 0;
  const uint32_t t0 = 0xFFFFFF00u;
  b.message(kCh, t0);
  Changes ch = run(b, t0, t0 + 2500, LedColor::Cyan, 15, true, last);
  TEST_ASSERT_EQUAL(12, int(ch.size()));
  expectDoubleBlink(ch, 0, t0, 6);
  expectDoubleBlink(ch, 4, t0 + 1000, 6);
  expectDoubleBlink(ch, 8, t0 + 2000, 6);
}

void test_sources_blink_in_turn() {
  NotifyBlinker b;
  uint8_t last = 0;
  NotifyInputs in;
  in.color[0] = LedColor::Blue;     // channel
  in.color[1] = LedColor::Yellow;   // mention
  in.color[2] = LedColor::Green;    // feed
  in.color[3] = LedColor::Magenta;  // C-Mail
  for (bool& u : in.unread) u = true;
  b.message(NotifySource::Cmail, 0);
  b.message(NotifySource::Channel, 0);
  b.message(NotifySource::Feed, 0);
  Changes ch = run(b, 0, 3800, in, 15, last);
  // Channel, feed, C-Mail in that order, 300 ms apart after each double
  // blink; the next round 700 ms after the last.
  TEST_ASSERT_EQUAL(4 * 6, int(ch.size()));
  expectDoubleBlink(ch, 0, 0, 4);
  expectDoubleBlink(ch, 4, 510, 2);
  expectDoubleBlink(ch, 8, 1020, 5);
  expectDoubleBlink(ch, 12, 1230 + 700, 4);
  expectDoubleBlink(ch, 16, 1930 + 510, 2);
  expectDoubleBlink(ch, 20, 1930 + 1020, 5);
}

void test_a_read_source_drops_out() {
  NotifyBlinker b;
  uint8_t last = 0;
  NotifyInputs in;
  in.color[0] = LedColor::Blue;
  in.color[3] = LedColor::Magenta;
  in.unread[0] = in.unread[3] = true;
  b.message(NotifySource::Channel, 0);
  b.message(NotifySource::Cmail, 0);
  Changes ch = run(b, 0, 100, in, 15, last);
  in.unread[0] = false;  // the channel is read mid-round: the round finishes
  ch = run(b, 100, 3000, in, 15, last);
  expectDoubleBlink(ch, 2, 510, 5);  // after the channel's second flash
  // Then C-Mail alone, each second.
  expectDoubleBlink(ch, 6, 720 + 700, 5);
  expectDoubleBlink(ch, 10, 1420 + 1000, 5);
  TEST_ASSERT_EQUAL(14, int(ch.size()));
}

void test_each_source_times_out_on_its_own() {
  NotifyBlinker b;
  uint8_t last = 0;
  NotifyInputs in;
  in.color[1] = LedColor::Yellow;
  in.color[2] = LedColor::Green;
  in.unread[1] = in.unread[2] = true;
  b.message(NotifySource::Mention, 0);
  run(b, 0, 10000, in, 15, last);
  b.message(NotifySource::Feed, 10000);
  run(b, 10000, 16000, in, 15, last);
  // From 15 s only the feed is left, until 25 s.
  Changes ch = run(b, 16000, 30000, in, 15, last);
  TEST_ASSERT_TRUE(ch.size() >= 4);
  for (auto& c : ch) TEST_ASSERT_TRUE(c.second == 0 || c.second == 2);
  TEST_ASSERT_TRUE(ch.back().first < 25000);
  TEST_ASSERT_FALSE(b.active());
}

void test_a_new_message_blinks_once_even_if_read() {
  NotifyBlinker b;
  uint8_t last = 0;
  b.message(NotifySource::Feed, 100);
  Changes ch = run(b, 100, 3000, only(NotifySource::Feed, LedColor::Green, false), 15, last);
  TEST_ASSERT_EQUAL(4, int(ch.size()));
  expectDoubleBlink(ch, 0, 100, 2);
  TEST_ASSERT_FALSE(b.active());
}

void test_off_skips_only_that_source() {
  NotifyBlinker b;
  uint8_t last = 0;
  NotifyInputs in;
  in.color[0] = LedColor::Off;
  in.color[3] = LedColor::Red;
  in.unread[0] = in.unread[3] = true;
  b.message(NotifySource::Channel, 0);
  b.message(NotifySource::Cmail, 0);
  Changes ch = run(b, 0, 1500, in, 15, last);
  expectDoubleBlink(ch, 0, 0, 1);
  expectDoubleBlink(ch, 4, 1000, 1);
  TEST_ASSERT_EQUAL(8, int(ch.size()));
}

void test_wifi_light_states() {
  TEST_ASSERT(wifiLightState(true, false) == WifiLight::Joined);
  TEST_ASSERT(wifiLightState(true, true) == WifiLight::Joined);  // joined wins over the AP
  TEST_ASSERT(wifiLightState(false, true) == WifiLight::SetupAp);
  TEST_ASSERT(wifiLightState(false, false) == WifiLight::Looking);
}

void test_led_levels() {
  TEST_ASSERT_EQUAL_UINT8(0, ledLevelValue(LedLevel::Off));
  // Each step brighter than the last; Medium is the old fixed 0x20.
  for (int i = 1; i < kLedLevelCount; ++i)
    TEST_ASSERT_TRUE(ledLevelValue(LedLevel(i)) > ledLevelValue(LedLevel(i - 1)));
  TEST_ASSERT_EQUAL_UINT8(0x20, ledLevelValue(LedLevel::Medium));
  LedLevel l = LedLevel::Medium;
  TEST_ASSERT_TRUE(parseLedLevel("dim", l));
  TEST_ASSERT_TRUE(l == LedLevel::Dim);
  TEST_ASSERT_FALSE(parseLedLevel("bright", l));
  TEST_ASSERT_TRUE(l == LedLevel::Dim);
  TEST_ASSERT_EQUAL_STRING("high", ledLevelKey(LedLevel::High));
  TEST_ASSERT_TRUE(std::u32string_view(ledLevelLabel(LedLevel::Low)) == U"Low");
  TEST_ASSERT_TRUE(ledLevelFromIndex(9) == LedLevel::Medium);
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
  RUN_TEST(test_sources_blink_in_turn);
  RUN_TEST(test_a_read_source_drops_out);
  RUN_TEST(test_each_source_times_out_on_its_own);
  RUN_TEST(test_a_new_message_blinks_once_even_if_read);
  RUN_TEST(test_off_skips_only_that_source);
  RUN_TEST(test_wifi_light_states);
  RUN_TEST(test_led_levels);
  return UNITY_END();
}
