#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/feed_fixtures.h"
#include "feed/feed_parse.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_iso_ms() {
  TEST_ASSERT_EQUAL_INT64(1774606321516LL, parseIsoMs("2026-03-27T10:12:01.516Z"));
  TEST_ASSERT_EQUAL_INT64(1774606321000LL, parseIsoMs("2026-03-27T10:12:01Z"));
  TEST_ASSERT_EQUAL_INT64(-1, parseIsoMs("yesterday"));
  TEST_ASSERT_EQUAL_INT64(0, parseIsoMs("1970-01-01T00:00:00Z"));
  TEST_ASSERT_EQUAL_INT64(-1, parseIsoMs("2026-13-27T10:12:01Z"));
  TEST_ASSERT_EQUAL_INT64(-1, parseIsoMs(""));
}

void test_parse_feed_page() {
  std::vector<Entry> v;
  std::string cursor;
  TEST_ASSERT_TRUE(parseFeedPage(fx::kFeedPage, v, cursor));
  TEST_ASSERT_EQUAL(3, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("p3", cursor.c_str());
  TEST_ASSERT_EQUAL_STRING("p1", v[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", v[0].author.c_str());
  TEST_ASSERT_EQUAL_STRING("first entry", v[0].body.c_str());
  TEST_ASSERT_EQUAL(2, v[0].replies);
  TEST_ASSERT_TRUE(v[0].isPublic);
  TEST_ASSERT_FALSE(v[0].nsfw);
  TEST_ASSERT_EQUAL_INT64(1774606321516LL, v[0].createdMs);
  const Entry& e = v[1];
  TEST_ASSERT_EQUAL_STRING("On spiders", e.title.c_str());
  TEST_ASSERT_EQUAL_STRING("on-spiders", e.slug.c_str());
  TEST_ASSERT_EQUAL(2, (int)e.topics.size());
  TEST_ASSERT_EQUAL_STRING("linux", e.topics[1].c_str());
  TEST_ASSERT_TRUE(e.nsfw);
  TEST_ASSERT_EQUAL_STRING("spiders", e.contentWarning.c_str());
  TEST_ASSERT_FALSE(e.edited);
  TEST_ASSERT_EQUAL_STRING("synths", v[2].guildSlug.c_str());

  v.clear();
  cursor = "stale";
  TEST_ASSERT_TRUE(parseFeedPage(fx::kFeedLast, v, cursor));
  TEST_ASSERT_EQUAL(1, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("", cursor.c_str());
}

void test_parse_entry_attachments_and_edit() {
  Entry e;
  TEST_ASSERT_TRUE(parseEntry(fx::kEntry, e));
  TEST_ASSERT_EQUAL_STRING("p5", e.id.c_str());
  TEST_ASSERT_TRUE(e.edited);
  TEST_ASSERT_FALSE(e.truncated);
  TEST_ASSERT_EQUAL(2, (int)e.attachments.size());
  TEST_ASSERT_EQUAL_STRING("[song] Boards \xE2\x80\x93 Roygbiv", e.attachments[0].c_str());
  TEST_ASSERT_EQUAL_STRING("[img]", e.attachments[1].c_str());
}

void test_parse_sparse_and_deleted() {
  Entry e;
  TEST_ASSERT_TRUE(parseEntry(fx::kEntrySparse, e));
  TEST_ASSERT_TRUE(e.deleted);
  TEST_ASSERT_TRUE(e.title.empty());
  TEST_ASSERT_EQUAL(0, (int)e.topics.size());
  TEST_ASSERT_EQUAL_STRING("[attachment]", e.attachments[0].c_str());
}

void test_parse_replies_threading() {
  std::vector<Reply> v;
  std::string cursor;
  TEST_ASSERT_TRUE(parseReplies(fx::kReplies, v, cursor));
  TEST_ASSERT_EQUAL(3, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("r3", cursor.c_str());
  TEST_ASSERT_EQUAL_STRING("r1", v[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("p1", v[0].postId.c_str());
  TEST_ASSERT_TRUE(v[0].parentId.empty());
  TEST_ASSERT_TRUE(v[1].edited);
  TEST_ASSERT_EQUAL_STRING("r1", v[2].parentId.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", v[2].parentAuthor.c_str());
  TEST_ASSERT_EQUAL_STRING("[song]", v[2].attachments[0].c_str());

  Reply r;
  TEST_ASSERT_TRUE(parseReply(fx::kReply, r));
  TEST_ASSERT_EQUAL_STRING("r7", r.id.c_str());
  TEST_ASSERT_EQUAL_STRING("p4", r.postId.c_str());
  TEST_ASSERT_EQUAL_STRING("a reply", r.body.c_str());
  TEST_ASSERT_TRUE(r.createdMs > 0);
}

void test_parse_created_ids() {
  std::string id;
  TEST_ASSERT_TRUE(parseCreatedId(fx::kCreatedPost, id));
  TEST_ASSERT_EQUAL_STRING("p10", id.c_str());
  TEST_ASSERT_TRUE(parseCreatedId(fx::kCreatedReply, id));
  TEST_ASSERT_EQUAL_STRING("r11", id.c_str());
  TEST_ASSERT_TRUE(parseCreatedId(fx::kCreatedBookmark, id));
  TEST_ASSERT_EQUAL_STRING("b12", id.c_str());
  TEST_ASSERT_FALSE(parseCreatedId("{\"data\":{\"slug\":\"x\"}}", id));
}

void test_parse_bookmarks_watches() {
  std::vector<Bookmark> b;
  std::string cursor = "stale";
  TEST_ASSERT_TRUE(parseBookmarks(fx::kBookmarks, b, cursor));
  TEST_ASSERT_EQUAL(2, (int)b.size());
  TEST_ASSERT_EQUAL_STRING("", cursor.c_str());
  TEST_ASSERT_EQUAL_STRING("b1", b[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("p1", b[0].postId.c_str());
  TEST_ASSERT_TRUE(b[0].replyId.empty());
  TEST_ASSERT_EQUAL_STRING("b2", b[1].id.c_str());
  TEST_ASSERT_EQUAL_STRING("r2", b[1].replyId.c_str());
  TEST_ASSERT_TRUE(b[1].postId.empty());

  std::vector<std::string> w;
  TEST_ASSERT_TRUE(parseWatches(fx::kWatches, w, cursor));
  TEST_ASSERT_EQUAL(2, (int)w.size());
  TEST_ASSERT_EQUAL_STRING("p1", w[0].c_str());
  TEST_ASSERT_EQUAL_STRING("p5", w[1].c_str());
  TEST_ASSERT_EQUAL_STRING("w2", cursor.c_str());

  bool watching = false;
  TEST_ASSERT_TRUE(parseWatching(fx::kWatching, watching));
  TEST_ASSERT_TRUE(watching);
}

void test_parse_rejects_garbage() {
  const char* bad[] = {"<html>", "{\"data\":null}"};
  for (const char* body : bad) {
    std::vector<Entry> es;
    std::vector<Reply> rs;
    std::vector<Bookmark> bs;
    std::vector<std::string> ws;
    std::string cursor, id;
    Entry e;
    Reply r;
    bool watching = false;
    TEST_ASSERT_FALSE(parseFeedPage(body, es, cursor));
    TEST_ASSERT_FALSE(parseEntry(body, e));
    TEST_ASSERT_FALSE(parseReplies(body, rs, cursor));
    TEST_ASSERT_FALSE(parseReply(body, r));
    TEST_ASSERT_FALSE(parseCreatedId(body, id));
    TEST_ASSERT_FALSE(parseBookmarks(body, bs, cursor));
    TEST_ASSERT_FALSE(parseWatches(body, ws, cursor));
    TEST_ASSERT_FALSE(parseWatching(body, watching));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_iso_ms);
  RUN_TEST(test_parse_feed_page);
  RUN_TEST(test_parse_entry_attachments_and_edit);
  RUN_TEST(test_parse_sparse_and_deleted);
  RUN_TEST(test_parse_replies_threading);
  RUN_TEST(test_parse_created_ids);
  RUN_TEST(test_parse_bookmarks_watches);
  RUN_TEST(test_parse_rejects_garbage);
  return UNITY_END();
}
