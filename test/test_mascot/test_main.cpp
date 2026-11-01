#include <ArduinoJson.h>
#include <unity.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/web/mascot_presets.h"
#include "../fixtures/mascot_golden.h"
#include "mascot/mascot.h"
#include "ui/screenshot.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static std::vector<uint8_t> fromHex(const char* hex) {
  std::vector<uint8_t> out;
  const char* p = hex;
  while (*p) {
    if (*p == ' ') {
      ++p;
      continue;
    }
    out.push_back(static_cast<uint8_t>(std::strtoul(std::string(p, 2).c_str(), nullptr, 16)));
    p += 2;
  }
  return out;
}

static Mascot sample() { return Mascot{{"/\\_/\\", "(o o)"}, {"boo", "meow"}}; }

// Rewrites the CRC after a header edit, so only the edited field is wrong.
static void reseal(std::vector<uint8_t>& b) {
  const size_t n = b[8] | (b[9] << 8);
  const uint32_t crc = crc32Update(0, b.data(), 10 + n);
  for (int i = 0; i < 4; ++i) b[10 + n + i] = uint8_t(crc >> (8 * i));
}

// A hand-built blob, bypassing encodeMascot's caller-validates contract.
static std::vector<uint8_t> rawBlob(const std::vector<std::string>& art,
                                    const std::vector<std::string>& phrases) {
  std::vector<uint8_t> b(kMascotBlobSize, 0xFF);
  std::vector<uint8_t> p;
  for (const auto* v : {&art, &phrases})
    for (const std::string& s : *v) {
      p.push_back(uint8_t(s.size()));
      p.insert(p.end(), s.begin(), s.end());
    }
  const uint8_t head[] = {'F', 'L', 'U', 'F', 1, uint8_t(art.size()), uint8_t(phrases.size()), 0,
                          uint8_t(p.size()), uint8_t(p.size() >> 8)};
  std::copy(head, head + 10, b.begin());
  std::copy(p.begin(), p.end(), b.begin() + 10);
  reseal(b);
  return b;
}

static bool decodes(const std::vector<uint8_t>& b) {
  Mascot m;
  return decodeMascot(b.data(), b.size(), m);
}

void test_golden() {
  std::vector<uint8_t> want = fromHex(fx::kMascotGoldenHex);
  std::vector<uint8_t> got = encodeMascot(sample());
  TEST_ASSERT_EQUAL(kMascotBlobSize, got.size());
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want.data(), got.data(), want.size());
  for (size_t i = want.size(); i < got.size(); ++i) TEST_ASSERT_EQUAL_HEX8(0xFF, got[i]);
}

void test_round_trip() {
  std::vector<uint8_t> b = encodeMascot(sample());
  Mascot m;
  TEST_ASSERT_TRUE(decodeMascot(b.data(), b.size(), m));
  TEST_ASSERT_TRUE(m.art == sample().art);
  TEST_ASSERT_TRUE(m.phrases == sample().phrases);
}

void test_bad_magic() {
  std::vector<uint8_t> b = encodeMascot(sample());
  b[0] = 'X';
  reseal(b);
  TEST_ASSERT_FALSE(decodes(b));
}

void test_bad_crc() {
  std::vector<uint8_t> b = encodeMascot(sample());
  b[12] ^= 1;
  TEST_ASSERT_FALSE(decodes(b));
}

void test_bad_version() {
  std::vector<uint8_t> b = encodeMascot(sample());
  b[4] = 2;
  reseal(b);
  TEST_ASSERT_FALSE(decodes(b));
  b[4] = 0;
  reseal(b);
  TEST_ASSERT_FALSE(decodes(b));
}

void test_erased_is_none() {
  std::vector<uint8_t> b(kMascotBlobSize, 0xFF);
  TEST_ASSERT_FALSE(decodes(b));
}

void test_short_buffer() {
  std::vector<uint8_t> b = encodeMascot(sample());
  Mascot m;
  TEST_ASSERT_FALSE(decodeMascot(b.data(), 20, m));
  b[8] = 0xFF;  // payload length past the buffer
  b[9] = 0xFF;
  TEST_ASSERT_FALSE(decodes(b));
}

void test_payload_must_end_at_n() {
  std::vector<uint8_t> b = rawBlob({"ab"}, {"hi"});
  b[8] += 1;  // one stray byte inside N
  b[10 + 6] = 'x';
  reseal(b);
  TEST_ASSERT_FALSE(decodes(b));
}

void test_reject_long_line() {
  Mascot m{{"12345678901234"}, {}};
  TEST_ASSERT_EQUAL_STRING("art line 1 is over 13 characters", validateMascot(m).c_str());
  TEST_ASSERT_FALSE(decodes(rawBlob(m.art, {})));
  TEST_ASSERT_TRUE(decodes(rawBlob({"1234567890123"}, {})));
}

void test_reject_seven_rows() {
  Mascot m{{"a", "b", "c", "d", "e", "f", "g"}, {}};
  TEST_ASSERT_TRUE(validateMascot(m) != "");
  TEST_ASSERT_FALSE(decodes(rawBlob(m.art, {})));
}

void test_reject_eleven_phrases() {
  Mascot m{{}, std::vector<std::string>(11, "hi")};
  TEST_ASSERT_TRUE(validateMascot(m) != "");
  TEST_ASSERT_FALSE(decodes(rawBlob({}, m.phrases)));
  m.phrases.pop_back();
  TEST_ASSERT_EQUAL_STRING("", validateMascot(m).c_str());
}

void test_reject_long_phrase() {
  Mascot m{{}, {std::string(81, 'x')}};
  TEST_ASSERT_TRUE(validateMascot(m) != "");
  TEST_ASSERT_FALSE(decodes(rawBlob({}, m.phrases)));
  TEST_ASSERT_TRUE(decodes(rawBlob({}, {std::string(80, 'x')})));
}

void test_reject_non_ascii() {
  Mascot del{{"a\x7f"}, {}};
  Mascot e{{}, {"caf\xc3\xa9"}};
  Mascot tab{{}, {"a\tb"}};
  TEST_ASSERT_TRUE(validateMascot(del) != "");
  TEST_ASSERT_TRUE(validateMascot(e) != "");
  TEST_ASSERT_TRUE(validateMascot(tab) != "");
  TEST_ASSERT_FALSE(decodes(rawBlob(del.art, {})));
  TEST_ASSERT_FALSE(decodes(rawBlob({}, e.phrases)));
}

void test_reject_empty_phrase() {
  Mascot m{{}, {""}};
  TEST_ASSERT_TRUE(validateMascot(m) != "");
  TEST_ASSERT_FALSE(decodes(rawBlob({}, m.phrases)));
}

void test_art_only() {
  Mascot in{{"(o o)"}, {}};
  TEST_ASSERT_EQUAL_STRING("", validateMascot(in).c_str());
  std::vector<uint8_t> b = encodeMascot(in);
  Mascot m;
  TEST_ASSERT_TRUE(decodeMascot(b.data(), b.size(), m));
  TEST_ASSERT_TRUE(m.art == in.art);
  TEST_ASSERT_TRUE(m.phrases.empty());
}

void test_phrases_only() {
  Mascot in{{}, {"meow"}};
  std::vector<uint8_t> b = encodeMascot(in);
  Mascot m;
  TEST_ASSERT_TRUE(decodeMascot(b.data(), b.size(), m));
  TEST_ASSERT_TRUE(m.art.empty());
  TEST_ASSERT_TRUE(m.phrases == in.phrases);
}

void test_trim_art() {
  Mascot m = cleanMascot(Mascot{{"ab  ", "", "c", "", ""}, {"", "hi", ""}});
  TEST_ASSERT_TRUE((m.art == std::vector<std::string>{"ab", "", "c"}));
  TEST_ASSERT_TRUE((m.phrases == std::vector<std::string>{"hi"}));
  TEST_ASSERT_TRUE(cleanMascot(Mascot{{" ", ""}, {}}).art.empty());
  TEST_ASSERT_TRUE((cleanMascot(Mascot{{"  x"}, {}}).art == std::vector<std::string>{"  x"}));
}

void test_defaults() {
  const auto& art = defaultMascotArt();
  TEST_ASSERT_EQUAL(6, art.size());
  size_t widest = 0;
  for (const auto& l : art) widest = std::max(widest, l.size());
  TEST_ASSERT_EQUAL(13, widest);
  const auto& ph = defaultMascotPhrases();
  TEST_ASSERT_EQUAL(6, ph.size());
  TEST_ASSERT_EQUAL_STRING("boo", ph[0].c_str());
  TEST_ASSERT_EQUAL_STRING("", validateMascot(Mascot{art, ph}).c_str());
}

static std::vector<std::string> strings(JsonArrayConst a) {
  std::vector<std::string> out;
  for (const char* s : a) out.push_back(s ? s : "");
  return out;
}

// site/mascots.json (through the generated header) must agree with the
// firmware's defaults, and every preset must fit the limits.
void test_presets_match_defaults() {
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, kMascotPresetsJson));
  JsonArrayConst presets = doc["presets"];
  TEST_ASSERT_EQUAL(5, presets.size());
  TEST_ASSERT_EQUAL_STRING("Fluffy", presets[0]["name"]);
  TEST_ASSERT_TRUE(strings(presets[0]["art"]) == defaultMascotArt());
  const std::vector<std::string> phrases = strings(doc["phrases"]);
  TEST_ASSERT_TRUE(phrases == defaultMascotPhrases());
  for (JsonObjectConst p : presets) {
    const Mascot m{strings(p["art"]), phrases};
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", validateMascot(m).c_str(), p["name"]);
    TEST_ASSERT_TRUE(cleanMascot(m).art == m.art);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_golden);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_bad_magic);
  RUN_TEST(test_bad_crc);
  RUN_TEST(test_bad_version);
  RUN_TEST(test_erased_is_none);
  RUN_TEST(test_short_buffer);
  RUN_TEST(test_payload_must_end_at_n);
  RUN_TEST(test_reject_long_line);
  RUN_TEST(test_reject_seven_rows);
  RUN_TEST(test_reject_eleven_phrases);
  RUN_TEST(test_reject_long_phrase);
  RUN_TEST(test_reject_non_ascii);
  RUN_TEST(test_reject_empty_phrase);
  RUN_TEST(test_art_only);
  RUN_TEST(test_phrases_only);
  RUN_TEST(test_trim_art);
  RUN_TEST(test_defaults);
  RUN_TEST(test_presets_match_defaults);
  return UNITY_END();
}
