#include <unity.h>

#include <string>
#include <vector>

#include "feed/feed_store.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static Entry E(const std::string& id) {
  Entry e;
  e.id = id;
  e.body = "body " + id;
  return e;
}

static Reply R(const std::string& id) {
  Reply r;
  r.id = id;
  r.body = "reply " + id;
  return r;
}

static std::vector<std::string> ids(const std::vector<Entry>& v) {
  std::vector<std::string> out;
  for (const auto& e : v) out.push_back(e.id);
  return out;
}

void test_first_page_prepends_new_keeps_cursor() {
  FeedStore s;
  s.mergeFirstPage({E("p3"), E("p2"), E("p1")}, "c1");
  Entry upd = E("p3");
  upd.replies = 7;
  s.mergeFirstPage({E("p5"), E("p4"), upd}, "x");
  TEST_ASSERT_TRUE((std::vector<std::string>{"p5", "p4", "p3", "p2", "p1"}) == ids(s.feed()));
  TEST_ASSERT_EQUAL_STRING("c1", s.feedCursor().c_str());
  TEST_ASSERT_EQUAL(7, s.feed()[2].replies);
  TEST_ASSERT_FALSE(s.feedEnd());
}

void test_first_page_without_overlap_replaces() {
  FeedStore s;
  s.mergeFirstPage({E("p3"), E("p2"), E("p1")}, "c1");
  s.mergeFirstPage({E("p9"), E("p8")}, "c8");
  TEST_ASSERT_TRUE((std::vector<std::string>{"p9", "p8"}) == ids(s.feed()));
  TEST_ASSERT_EQUAL_STRING("c8", s.feedCursor().c_str());
}

void test_append_page_dedupes_and_end() {
  FeedStore s;
  TEST_ASSERT_FALSE(s.feedEnd());
  s.mergeFirstPage({E("p5"), E("p4"), E("p3")}, "c3");
  s.appendPage({E("p3"), E("p2"), E("p1")}, "");
  TEST_ASSERT_TRUE((std::vector<std::string>{"p5", "p4", "p3", "p2", "p1"}) == ids(s.feed()));
  TEST_ASSERT_TRUE(s.feedEnd());
}

void test_preview_cut() {
  FeedStore s;
  Entry e = E("p1");
  e.body.clear();
  for (int i = 0; i < 300; ++i) e.body += "é";
  s.mergeFirstPage({e}, "");
  TEST_ASSERT_EQUAL(280, (int)utf8Length(s.feed()[0].body));
  TEST_ASSERT_TRUE(s.feed()[0].truncated);

  s.setSideList({e}, "", false);
  TEST_ASSERT_EQUAL(280, (int)utf8Length(s.sideList()[0].body));
  TEST_ASSERT_TRUE(s.sideList()[0].truncated);
}

void test_entry_and_reply_caps() {
  FeedStore s;
  Entry e = E("p1");
  e.body.assign(20000, 'a');
  s.openEntry(e);
  TEST_ASSERT_NOT_NULL(s.entry());
  TEST_ASSERT_EQUAL(16384, (int)s.entry()->body.size());
  TEST_ASSERT_TRUE(s.entry()->truncated);

  Reply r = R("r1");
  r.body.assign(5000, 'b');
  s.appendReplies({r, R("r2")}, "c");
  TEST_ASSERT_EQUAL(2, (int)s.replies().size());
  TEST_ASSERT_EQUAL(4096, (int)s.replies()[0].body.size());
  TEST_ASSERT_TRUE(s.replies()[0].truncated);
  TEST_ASSERT_FALSE(s.replies()[1].truncated);

  s.appendReplies({R("r2"), R("r3")}, "");
  TEST_ASSERT_EQUAL(3, (int)s.replies().size());
  TEST_ASSERT_TRUE(s.repliesEnd());
}

void test_reply_cap_drops_oldest() {
  FeedStore s;
  s.openEntry(E("p1"));
  int n = 0;
  for (int page = 0; page < 6; ++page) {
    std::vector<Reply> v;
    for (int i = 0; i < 20; ++i) v.push_back(R("r" + std::to_string(++n)));
    s.appendReplies(std::move(v), "c" + std::to_string(page));
  }
  TEST_ASSERT_EQUAL(100, (int)s.replies().size());
  TEST_ASSERT_EQUAL_STRING("r21", s.replies().front().id.c_str());
  TEST_ASSERT_EQUAL_STRING("r120", s.replies().back().id.c_str());
}

void test_resume_cursor() {
  FeedStore s;
  s.openEntry(E("p1"));
  TEST_ASSERT_EQUAL_STRING("", s.repliesResumeCursor().c_str());
  s.appendReplies({R("r1"), R("r2"), R("r3")}, "r3");
  TEST_ASSERT_EQUAL_STRING("r3", s.repliesResumeCursor().c_str());
  s.appendReplies({R("r4"), R("r5"), R("r6"), R("r7")}, "");
  TEST_ASSERT_EQUAL_STRING("r7", s.repliesResumeCursor().c_str());

  s.setWatching(true);
  TEST_ASSERT_TRUE(s.watching());
  s.openEntry(E("p2"));
  TEST_ASSERT_FALSE(s.watching());
  TEST_ASSERT_EQUAL(0, (int)s.replies().size());
  TEST_ASSERT_FALSE(s.repliesEnd());
  TEST_ASSERT_EQUAL_STRING("", s.repliesResumeCursor().c_str());
}

void test_bookmark_map() {
  FeedStore s;
  TEST_ASSERT_FALSE(s.bookmarksLoaded());
  Bookmark b1{"b1", "p1", ""};
  Bookmark b2{"b2", "", "r1"};
  s.setBookmarks({b1, b2});
  TEST_ASSERT_TRUE(s.bookmarksLoaded());
  TEST_ASSERT_EQUAL_STRING("b1", s.bookmarkIdFor("p1").c_str());
  TEST_ASSERT_EQUAL_STRING("b2", s.bookmarkIdFor("r1").c_str());
  s.removeBookmark("p1");
  TEST_ASSERT_EQUAL_STRING("", s.bookmarkIdFor("p1").c_str());
  s.addBookmark(Bookmark{"b3", "p2", ""});
  TEST_ASSERT_EQUAL_STRING("b3", s.bookmarkIdFor("p2").c_str());
  TEST_ASSERT_EQUAL_STRING("", s.bookmarkIdFor("zz").c_str());
}

void test_mark_deleted() {
  FeedStore s;
  s.mergeFirstPage({E("p2"), E("p1")}, "c");
  s.openEntry(E("p1"));
  s.appendReplies({R("r1"), R("r2")}, "");
  s.markDeleted("p1");
  s.markDeleted("r2");
  TEST_ASSERT_TRUE(s.feed()[1].deleted);
  TEST_ASSERT_FALSE(s.feed()[0].deleted);
  TEST_ASSERT_TRUE(s.entry()->deleted);
  TEST_ASSERT_FALSE(s.replies()[0].deleted);
  TEST_ASSERT_TRUE(s.replies()[1].deleted);
}

void test_index_of() {
  FeedStore s;
  s.mergeFirstPage({E("p3"), E("p2"), E("p1")}, "c1");
  TEST_ASSERT_EQUAL(1, s.indexOf("p2"));
  s.mergeFirstPage({E("p5"), E("p4"), E("p3")}, "x");
  TEST_ASSERT_EQUAL(3, s.indexOf("p2"));
  TEST_ASSERT_EQUAL(-1, s.indexOf("nope"));
}

void test_clear_drops_everything() {
  FeedStore s;
  s.mergeFirstPage({E("p1")}, "c");
  s.openEntry(E("p1"));
  s.appendReplies({R("r1")}, "");
  s.setBookmarks({Bookmark{"b1", "p1", ""}});
  s.setSideList({E("p1")}, "sc", false);
  s.setWatching(true);
  s.clear();
  TEST_ASSERT_EQUAL(0, (int)s.feed().size());
  TEST_ASSERT_NULL(s.entry());
  TEST_ASSERT_EQUAL(0, (int)s.replies().size());
  TEST_ASSERT_EQUAL_STRING("", s.bookmarkIdFor("p1").c_str());
  TEST_ASSERT_FALSE(s.bookmarksLoaded());
  TEST_ASSERT_EQUAL(0, (int)s.sideList().size());
  TEST_ASSERT_EQUAL_STRING("", s.sideCursor().c_str());
  TEST_ASSERT_FALSE(s.watching());
  TEST_ASSERT_FALSE(s.feedEnd());
}

void test_side_list_append() {
  FeedStore s;
  s.setSideList({E("p1"), E("p2")}, "c2", false);
  s.setSideList({E("p3")}, "", true);
  TEST_ASSERT_TRUE((std::vector<std::string>{"p1", "p2", "p3"}) == ids(s.sideList()));
  TEST_ASSERT_EQUAL_STRING("", s.sideCursor().c_str());
  s.setSideList({E("p9")}, "c9", false);
  TEST_ASSERT_TRUE((std::vector<std::string>{"p9"}) == ids(s.sideList()));
  TEST_ASSERT_EQUAL_STRING("c9", s.sideCursor().c_str());
}

void test_side_list_reply_ids() {
  FeedStore s;
  s.setSideList({E("p1"), E("p2")}, "c2", false, {"", "r4"});
  s.setSideList({E("p3")}, "", true);
  TEST_ASSERT_TRUE((std::vector<std::string>{"", "r4", ""}) == s.sideReplyIds());
  s.setSideList({E("p9")}, "", false);
  TEST_ASSERT_TRUE((std::vector<std::string>{""}) == s.sideReplyIds());
  s.setSideList({E("p5")}, "", false, {"r5"});
  s.clear();
  TEST_ASSERT_EQUAL(0, (int)s.sideReplyIds().size());
}

void test_close_entry() {
  FeedStore s;
  s.openEntry(E("p1"));
  s.appendReplies({R("r1")}, "");
  s.closeEntry();
  TEST_ASSERT_NULL(s.entry());
  TEST_ASSERT_EQUAL(0, (int)s.replies().size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_page_prepends_new_keeps_cursor);
  RUN_TEST(test_first_page_without_overlap_replaces);
  RUN_TEST(test_append_page_dedupes_and_end);
  RUN_TEST(test_preview_cut);
  RUN_TEST(test_entry_and_reply_caps);
  RUN_TEST(test_reply_cap_drops_oldest);
  RUN_TEST(test_resume_cursor);
  RUN_TEST(test_bookmark_map);
  RUN_TEST(test_mark_deleted);
  RUN_TEST(test_index_of);
  RUN_TEST(test_clear_drops_everything);
  RUN_TEST(test_side_list_append);
  RUN_TEST(test_side_list_reply_ids);
  RUN_TEST(test_close_entry);
  return UNITY_END();
}
