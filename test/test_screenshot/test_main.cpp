#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "ui/screenshot.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static std::vector<uint16_t> decode(const uint8_t* b, size_t n) {
  std::vector<uint16_t> px;
  for (size_t i = 0; i + 3 <= n; i += 3) {
    for (int k = 0; k < b[i]; ++k) px.push_back(uint16_t(b[i + 1] | b[i + 2] << 8));
  }
  return px;
}

void test_runs_round_trip() {
  const uint16_t row[] = {1, 1, 1, 0xBEEF, 2, 2};
  uint8_t out[3 * 6];
  const size_t n = rle565Row(row, 6, out);
  TEST_ASSERT_EQUAL(9, n);
  TEST_ASSERT_EQUAL(3, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0xEF, out[4]);
  TEST_ASSERT_EQUAL_HEX8(0xBE, out[5]);
  std::vector<uint16_t> back = decode(out, n);
  TEST_ASSERT_EQUAL(6, back.size());
  TEST_ASSERT_EQUAL_MEMORY(row, back.data(), sizeof(row));
}

void test_long_runs_split_at_255() {
  std::vector<uint16_t> row(600, 0x1234);
  std::vector<uint8_t> out(3 * row.size());
  const size_t n = rle565Row(row.data(), static_cast<int>(row.size()), out.data());
  TEST_ASSERT_EQUAL(9, n);
  TEST_ASSERT_EQUAL(255, out[0]);
  TEST_ASSERT_EQUAL(255, out[3]);
  TEST_ASSERT_EQUAL(90, out[6]);
}

void test_crc32_matches_zlib() {
  const char* s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32Update(0, reinterpret_cast<const uint8_t*>(s), 9));
  uint32_t c = crc32Update(0, reinterpret_cast<const uint8_t*>(s), 4);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32Update(c, reinterpret_cast<const uint8_t*>(s + 4), 5));
}

void test_command_line() {
  SerialCommand cmd;
  const char* in = "junk\r\nscreenshot\r\nscreenshotx\nscreenshot\n";
  int hits = 0;
  for (const char* p = in; *p; ++p)
    if (cmd.feed(*p) == SerialCommand::Screenshot) ++hits;
  TEST_ASSERT_EQUAL(2, hits);
  for (int i = 0; i < 100; ++i) cmd.feed('a');  // an overlong line is dropped
  int late = 0;
  for (const char* p = "\nscreenshot\n"; *p; ++p)
    if (cmd.feed(*p) == SerialCommand::Screenshot) ++late;
  TEST_ASSERT_EQUAL(1, late);
}

void test_key_and_type_commands() {
  SerialCommand c;
  const char* names[] = {"up", "down", "left", "right", "select", "back", "home", "palette", "options",
                         "backspace", "symbols", "emoji", "circ", "cmail", "feed", "screenoff"};
  const Action acts[] = {Action::Up, Action::Down, Action::Left, Action::Right, Action::Select,
                         Action::Back, Action::OpenHome, Action::Palette, Action::Options,
                         Action::Backspace, Action::Symbols, Action::Emoji, Action::OpenCirc,
                         Action::OpenCmail, Action::OpenFeed, Action::ScreenOff};
  for (size_t i = 0; i < sizeof(acts) / sizeof(acts[0]); ++i) {
    std::string line = std::string("key ") + names[i] + "\n";
    SerialCommand::Result r = SerialCommand::None;
    for (char ch : line) r = c.feed(ch);
    TEST_ASSERT_EQUAL(SerialCommand::Key, r);
    TEST_ASSERT_EQUAL(static_cast<int>(acts[i]), static_cast<int>(c.key().action));
  }
  SerialCommand::Result r = SerialCommand::None;
  for (char ch : std::string("key nonsense\n")) r = c.feed(ch);
  TEST_ASSERT_EQUAL(SerialCommand::None, r);
  for (char ch : std::string("type hi there\n")) r = c.feed(ch);
  TEST_ASSERT_EQUAL(SerialCommand::Type, r);
  TEST_ASSERT_EQUAL_STRING("hi there", std::string(c.text()).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_runs_round_trip);
  RUN_TEST(test_long_runs_split_at_255);
  RUN_TEST(test_crc32_matches_zlib);
  RUN_TEST(test_command_line);
  RUN_TEST(test_key_and_type_commands);
  return UNITY_END();
}
