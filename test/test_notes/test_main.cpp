#include <ArduinoJson.h>
#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/notes_fixtures.h"
#include "feed/feed_parse.h"
#include "notes/notes.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_note_list_parse() {
  std::vector<NoteRow> rows;
  std::string cur;
  TEST_ASSERT_TRUE(parseNoteList(fx::kNoteList, rows, cur));
  TEST_ASSERT_EQUAL(2, int(rows.size()));
  TEST_ASSERT_EQUAL_STRING("n1", rows[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("Shopping", rows[0].title.c_str());
  TEST_ASSERT_EQUAL(3, rows[0].revision);
  TEST_ASSERT_TRUE(rows[0].updatedMs == parseIsoMs("2026-11-02T10:00:00.000Z"));
  TEST_ASSERT_EQUAL_STRING("Plain idea", rows[1].title.c_str());
  TEST_ASSERT_EQUAL_STRING("n2", cur.c_str());
}

void test_note_list_missing_fields() {
  std::vector<NoteRow> rows;
  std::string cur = "x";
  TEST_ASSERT_TRUE(parseNoteList(fx::kNoteListBare, rows, cur));
  TEST_ASSERT_EQUAL(1, int(rows.size()));
  TEST_ASSERT_TRUE(rows[0].updatedMs == -1);
  TEST_ASSERT_EQUAL(0, rows[0].revision);
  TEST_ASSERT_EQUAL_STRING("", cur.c_str());
}

void test_note_list_drops_bodies() {
  std::string big(32768, 'a');
  std::string body = R"({"data":[{"noteId":"n","content":")" + big + R"(","deleted":false}],"cursor":null})";
  std::vector<NoteRow> rows;
  std::string cur;
  TEST_ASSERT_TRUE(parseNoteList(body, rows, cur));
  TEST_ASSERT_TRUE(rows[0].title.size() <= kNoteTitleBytes);
}

void test_note_parse() {
  Note n;
  TEST_ASSERT_TRUE(parseNote(fx::kNote, n));
  TEST_ASSERT_EQUAL_STRING("n1", n.id.c_str());
  TEST_ASSERT_EQUAL_STRING("# Shopping\nmilk\neggs", n.content.c_str());
  TEST_ASSERT_EQUAL(3, n.revision);
  TEST_ASSERT_TRUE(n.updatedMs == parseIsoMs("2026-11-02T10:00:00.000Z"));
  Note m;
  TEST_ASSERT_FALSE(parseNote(fx::kNoteNoId, m));
}

void test_note_id_parse() {
  std::string id;
  TEST_ASSERT_TRUE(parseNoteId(fx::kNoteCreated, id));
  TEST_ASSERT_EQUAL_STRING("n9", id.c_str());
}

void test_note_first_line_rules() {
  TEST_ASSERT_EQUAL_STRING("Shopping", noteTitle("\n\n# Shopping\nmilk").c_str());
  TEST_ASSERT_EQUAL_STRING("Deep", noteTitle("  ### Deep  \n").c_str());
  TEST_ASSERT_EQUAL_STRING("", noteTitle("   \n").c_str());
  std::string e;
  for (int i = 0; i < 200; ++i) e += "é";
  std::string t = noteTitle(e);
  TEST_ASSERT_TRUE(t.size() <= kNoteTitleBytes);
  TEST_ASSERT_TRUE(t.size() > 100);
  TEST_ASSERT_EQUAL(int(t.size() / 2), int(utf8ToU32(t).size()));
}

void test_note_body_escapes() {
  std::string c = "a \"q\" \\ b\nc\td\x01";
  std::string j = noteBodyJson(c);
  JsonDocument d;
  TEST_ASSERT_FALSE(deserializeJson(d, j));
  TEST_ASSERT_EQUAL_STRING(c.c_str(), d["content"].as<const char*>());
  TEST_ASSERT_EQUAL(1, int(d.as<JsonObjectConst>().size()));
}

void test_note_error() {
  TEST_ASSERT_EQUAL_STRING("Note is empty", noteError(""));
  TEST_ASSERT_EQUAL_STRING("Note is empty", noteError(" \n"));
  TEST_ASSERT_NULL(noteError(std::string(32768, 'a')));
  TEST_ASSERT_EQUAL_STRING("Note is too long", noteError(std::string(32769, 'a')));
  std::string s;
  for (int i = 0; i < 32768; ++i) s += "🙂";
  TEST_ASSERT_NULL(noteError(s));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_note_list_parse);
  RUN_TEST(test_note_list_missing_fields);
  RUN_TEST(test_note_list_drops_bodies);
  RUN_TEST(test_note_parse);
  RUN_TEST(test_note_id_parse);
  RUN_TEST(test_note_first_line_rules);
  RUN_TEST(test_note_body_escapes);
  RUN_TEST(test_note_error);
  return UNITY_END();
}
