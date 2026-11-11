#include <ArduinoJson.h>
#include <unity.h>

#include <string>
#include <vector>

#include "feed/compose.h"
#include "feed/entry.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_split_topics() {
  auto t = splitTopics(" Music, #Linux ,, ");
  TEST_ASSERT_EQUAL(2, (int)t.size());
  TEST_ASSERT_EQUAL_STRING("music", t[0].c_str());
  TEST_ASSERT_EQUAL_STRING("linux", t[1].c_str());
  TEST_ASSERT_EQUAL(0, (int)splitTopics(" # , ").size());
}

void test_validate_limits() {
  EntryDraft d;
  d.body = "hi";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
  d.title = std::string(100, 'a');
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
  d.title = std::string(101, 'a');
  TEST_ASSERT_EQUAL_STRING("Title over 100 characters", validateDraft(d).c_str());
  d.title = "";
  d.topics = "a,b,c";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
  d.topics = "a,b,c,d";
  TEST_ASSERT_EQUAL_STRING("At most 3 topics", validateDraft(d).c_str());
  d.topics = "";
  d.cw = std::string(60, 'w');
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
  d.cw = std::string(61, 'w');
  TEST_ASSERT_EQUAL_STRING("Warning over 60 characters", validateDraft(d).c_str());
  d.cw = "";
  d.body.clear();
  for (int i = 0; i < 4096; ++i) d.body += "é";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());  // 4096 code points, 8192 bytes
  d.body += "é";
  TEST_ASSERT_EQUAL_STRING("Body over 4096 characters", validateDraft(d).c_str());
}

void test_validate_counts_code_points() {
  EntryDraft d;
  d.body = "hi";
  d.title.clear();
  for (int i = 0; i < 100; ++i) d.title += "é";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
  d.title.clear();
  for (int i = 0; i < 60; ++i) d.cw += "é";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
}

void test_validate_order() {
  EntryDraft d;
  d.title = std::string(101, 'a');
  d.topics = "a,b,c,d";
  d.cw = std::string(61, 'w');
  TEST_ASSERT_EQUAL_STRING("Body is empty", validateDraft(d).c_str());
  d.body = "x";
  TEST_ASSERT_EQUAL_STRING("Title over 100 characters", validateDraft(d).c_str());
  d.title = "";
  TEST_ASSERT_EQUAL_STRING("At most 3 topics", validateDraft(d).c_str());
}

void test_whitespace_body_rejected() {
  EntryDraft d;
  d.body = " \n\t ";
  TEST_ASSERT_EQUAL_STRING("Body is empty", validateDraft(d).c_str());
}

void test_whitespace_reply_rejected() {
  TEST_ASSERT_EQUAL_STRING("Reply is empty", validateReply("").c_str());
  TEST_ASSERT_EQUAL_STRING("Reply is empty", validateReply(" \t\n ").c_str());
  TEST_ASSERT_EQUAL_STRING("", validateReply("  hi ").c_str());
}

void test_entry_json_omits_empties() {
  EntryDraft d;
  d.body = "hi";
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", entryJson(d).c_str());
  d.topics = " , #";
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", entryJson(d).c_str());
  d.title = "T";
  d.topics = "Music";
  d.cw = "cw";
  d.isPublic = true;
  d.nsfw = true;
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, entryJson(d)));
  TEST_ASSERT_EQUAL_STRING("hi", doc["content"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("T", doc["title"].as<const char*>());
  TEST_ASSERT_EQUAL(1, (int)doc["topics"].size());
  TEST_ASSERT_EQUAL_STRING("music", doc["topics"][0].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("cw", doc["contentWarning"].as<const char*>());
  TEST_ASSERT_TRUE(doc["isPublic"].as<bool>());
  TEST_ASSERT_TRUE(doc["isNSFW"].as<bool>());
}

void test_reply_flag_bookmark_json() {
  TEST_ASSERT_EQUAL_STRING("{\"postId\":\"p1\",\"content\":\"yo\"}",
                           replyJson("p1", "yo", "").c_str());
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, replyJson("p1", "yo", "r1")));
  TEST_ASSERT_EQUAL_STRING("p1", doc["postId"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("yo", doc["content"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("r1", doc["parentReplyId"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("{}", flagJson("").c_str());
  TEST_ASSERT_EQUAL_STRING("{\"reason\":\"spam \\\"x\\\"\"}", flagJson("spam \"x\"").c_str());
  TEST_ASSERT_EQUAL_STRING("{\"postId\":\"p1\",\"type\":\"post\"}", bookmarkJson("p1", "").c_str());
  TEST_ASSERT_EQUAL_STRING("{\"replyId\":\"r1\",\"type\":\"reply\"}", bookmarkJson("", "r1").c_str());
}

void test_title_and_cw_trimmed() {
  EntryDraft d;
  d.body = "hi";
  d.title = " \t ";
  d.cw = "  \n";
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", entryJson(d).c_str());
  d.title = "  T ";
  d.cw = " cw  ";
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, entryJson(d)));
  TEST_ASSERT_EQUAL_STRING("T", doc["title"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("cw", doc["contentWarning"].as<const char*>());
  // Lengths count the trimmed text.
  d.title = " " + std::string(100, 'a') + " ";
  d.cw = " " + std::string(60, 'w') + " ";
  TEST_ASSERT_EQUAL_STRING("", validateDraft(d).c_str());
}

void test_guild_thread_json_minimal() {
  EntryDraft d;
  d.body = "hi";
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", guildThreadJson(d).c_str());
}

void test_guild_thread_json_full() {
  EntryDraft d;
  d.body = "hi";
  d.title = "  T ";
  d.topics = "A, #b";
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, guildThreadJson(d)));
  TEST_ASSERT_EQUAL_STRING("T", doc["title"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("a", doc["topics"][0].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("b", doc["topics"][1].as<const char*>());
  TEST_ASSERT_EQUAL(2, doc["topics"].size());
}

void test_guild_thread_json_ignores_flags() {
  EntryDraft d;
  d.body = "hi";
  d.isPublic = true;
  d.nsfw = true;
  d.cw = "warn";
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi\"}", guildThreadJson(d).c_str());
}

void test_entry_patch_json_sends_everything() {
  EntryDraft d;
  d.body = "new body";
  d.title = "  ";
  d.topics = "";
  d.cw = "";
  JsonDocument doc;
  deserializeJson(doc, entryPatchJson(d, false));
  TEST_ASSERT_EQUAL_STRING("new body", doc["content"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("", doc["title"].as<const char*>());
  TEST_ASSERT_TRUE(doc["topics"].is<JsonArray>());
  TEST_ASSERT_EQUAL(0, doc["topics"].size());
  TEST_ASSERT_EQUAL_STRING("", doc["contentWarning"].as<const char*>());
  TEST_ASSERT_TRUE(doc["isPublic"].is<bool>());
  TEST_ASSERT_FALSE(doc["isPublic"].as<bool>());
  TEST_ASSERT_TRUE(doc["isNSFW"].is<bool>());
  TEST_ASSERT_FALSE(doc["isNSFW"].as<bool>());
  TEST_ASSERT_FALSE(doc["slug"].is<const char*>());
}

void test_entry_patch_json_full() {
  EntryDraft d;
  d.body = "b";
  d.title = " T ";
  d.topics = "#A, b";
  d.cw = " cw ";
  d.isPublic = true;
  d.nsfw = true;
  JsonDocument doc;
  deserializeJson(doc, entryPatchJson(d, false));
  TEST_ASSERT_EQUAL_STRING("T", doc["title"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("a", doc["topics"][0].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("b", doc["topics"][1].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("cw", doc["contentWarning"].as<const char*>());
  TEST_ASSERT_TRUE(doc["isPublic"].as<bool>());
  TEST_ASSERT_TRUE(doc["isNSFW"].as<bool>());
}

void test_entry_patch_json_guild_thread() {
  EntryDraft d;
  d.body = "b";
  d.cw = "warn";
  d.isPublic = true;
  d.nsfw = true;
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"b\",\"title\":\"\",\"topics\":[]}",
                           entryPatchJson(d, true).c_str());
}

void test_reply_patch_json() {
  TEST_ASSERT_EQUAL_STRING("{\"content\":\"hi \\\"x\\\"\"}", replyPatchJson("hi \"x\"").c_str());
}

void test_edit_window() {
  const int64_t t = 1800000000000LL;  // 2027
  TEST_ASSERT_TRUE(editWindowOpen(t, t));
  TEST_ASSERT_TRUE(editWindowOpen(t, t + 5 * 60000));
  TEST_ASSERT_FALSE(editWindowOpen(t, t + 5 * 60000 + 1));
  TEST_ASSERT_TRUE(editWindowOpen(t, t - 30000));       // our clock a little behind
  TEST_ASSERT_TRUE(editWindowOpen(t, 5000));            // clock not set: let the API decide
  TEST_ASSERT_TRUE(editWindowOpen(-1, t));              // no publish time
}

void test_draft_from_entry() {
  Entry e;
  e.title = "T";
  e.body = "body";
  e.topics = {"a", "b"};
  e.contentWarning = "cw";
  e.isPublic = true;
  e.nsfw = true;
  EntryDraft d = draftFromEntry(e);
  TEST_ASSERT_EQUAL_STRING("T", d.title.c_str());
  TEST_ASSERT_EQUAL_STRING("body", d.body.c_str());
  TEST_ASSERT_EQUAL_STRING("a, b", d.topics.c_str());
  TEST_ASSERT_EQUAL_STRING("cw", d.cw.c_str());
  TEST_ASSERT_TRUE(d.isPublic);
  TEST_ASSERT_TRUE(d.nsfw);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_split_topics);
  RUN_TEST(test_validate_limits);
  RUN_TEST(test_validate_counts_code_points);
  RUN_TEST(test_validate_order);
  RUN_TEST(test_whitespace_body_rejected);
  RUN_TEST(test_whitespace_reply_rejected);
  RUN_TEST(test_entry_json_omits_empties);
  RUN_TEST(test_reply_flag_bookmark_json);
  RUN_TEST(test_title_and_cw_trimmed);
  RUN_TEST(test_guild_thread_json_minimal);
  RUN_TEST(test_guild_thread_json_full);
  RUN_TEST(test_guild_thread_json_ignores_flags);
  RUN_TEST(test_entry_patch_json_sends_everything);
  RUN_TEST(test_entry_patch_json_full);
  RUN_TEST(test_entry_patch_json_guild_thread);
  RUN_TEST(test_reply_patch_json);
  RUN_TEST(test_edit_window);
  RUN_TEST(test_draft_from_entry);
  return UNITY_END();
}
