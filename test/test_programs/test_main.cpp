// SPDX-License-Identifier: GPL-3.0-or-later
#include <ArduinoJson.h>
#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/programs_fixtures.h"
#include "programs/programs.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_program_page_parse() {
  std::vector<ProgramInfo> rows;
  std::string cur;
  TEST_ASSERT_TRUE(parseProgramPage(fx::kProgramPage, rows, cur));
  TEST_ASSERT_EQUAL(2, int(rows.size()));
  TEST_ASSERT_EQUAL_STRING("p1", rows[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("hello", rows[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("ada", rows[0].owner.c_str());
  TEST_ASSERT_EQUAL_STRING("term", rows[0].runtime.c_str());
  TEST_ASSERT_EQUAL(3, rows[0].release);
  TEST_ASSERT_TRUE(rows[0].publishedMs == 1762000000000LL);
  TEST_ASSERT_EQUAL_STRING("web", rows[1].runtime.c_str());  // missing reads as web
  TEST_ASSERT_EQUAL_STRING("1761000000000", cur.c_str());
}

void test_program_page_null_cursor() {
  std::vector<ProgramInfo> rows{ProgramInfo{}};
  std::string cur = "x";
  TEST_ASSERT_TRUE(parseProgramPage(fx::kProgramPageEnd, rows, cur));
  TEST_ASSERT_EQUAL(0, int(rows.size()));
  TEST_ASSERT_EQUAL_STRING("", cur.c_str());
  TEST_ASSERT_TRUE(parseProgramPage(R"({"data":[],"cursor":"abc"})", rows, cur));
  TEST_ASSERT_EQUAL_STRING("abc", cur.c_str());
  TEST_ASSERT_FALSE(parseProgramPage("{}", rows, cur));
}

void test_program_source_utf8() {
  ProgramSource s;
  TEST_ASSERT_TRUE(parseProgramSource(fx::kProgramSourceUtf8, s));
  TEST_ASSERT_EQUAL_STRING("p1", s.info.id.c_str());
  TEST_ASSERT_EQUAL(3, s.info.release);
  TEST_ASSERT_FALSE(s.binary);
  TEST_ASSERT_FALSE(s.cut);
  TEST_ASSERT_EQUAL_STRING("export default async (p) => {\n\tp.print('hi')\n}\n", s.text.c_str());
  TEST_ASSERT_EQUAL(int(s.text.size()), int(s.bytes));
}

void test_program_source_wasm_size() {
  ProgramSource s;
  TEST_ASSERT_TRUE(parseProgramSource(fx::kProgramSourceWasm, s));
  TEST_ASSERT_TRUE(s.binary);
  TEST_ASSERT_EQUAL(8, int(s.bytes));
  TEST_ASSERT_TRUE(s.text.empty());
  TEST_ASSERT_EQUAL(0, int(base64DecodedSize("")));
  TEST_ASSERT_EQUAL(1, int(base64DecodedSize("QQ==")));
  TEST_ASSERT_EQUAL(2, int(base64DecodedSize("QUI=")));
  TEST_ASSERT_EQUAL(6, int(base64DecodedSize("QUJD\nREVG")));
}

void test_program_source_cut_on_utf8_boundary() {
  // 'é' is two bytes; one ASCII byte first puts a boundary at the cap.
  std::string src = "a";
  while (src.size() < kSourceShowMax + 10) src += "\xC3\xA9";
  JsonDocument d;
  d["data"]["id"] = "p1";
  d["data"]["encoding"] = "utf8";
  d["data"]["source"] = src;
  std::string body;
  serializeJson(d, body);
  ProgramSource s;
  TEST_ASSERT_TRUE(parseProgramSource(body, s));
  TEST_ASSERT_TRUE(s.cut);
  TEST_ASSERT_EQUAL(int(src.size()), int(s.bytes));
  TEST_ASSERT_TRUE(s.text.size() <= kSourceShowMax);
  TEST_ASSERT_TRUE(s.text.size() >= kSourceShowMax - 1);
  TEST_ASSERT_EQUAL(0, int((s.text.size() - 1) % 2));  // never half an é
  TEST_ASSERT_FALSE(parseProgramSource(R"({"data":{"name":"x"}})", s));
}

void test_clean_source() {
  TEST_ASSERT_TRUE(cleanSource(U"a\tb\r\nline two\n\x01x\n") == U"a  b\nline two\nx");
  TEST_ASSERT_TRUE(cleanSource(U"") == U"");
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_program_page_parse);
  RUN_TEST(test_program_page_null_cursor);
  RUN_TEST(test_program_source_utf8);
  RUN_TEST(test_program_source_wasm_size);
  RUN_TEST(test_program_source_cut_on_utf8_boundary);
  RUN_TEST(test_clean_source);
  return UNITY_END();
}
