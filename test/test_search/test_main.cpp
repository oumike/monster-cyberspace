// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/search_fixtures.h"
#include "search/search.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_all_groups_parse() {
  std::vector<SearchHit> users, posts, replies;
  TEST_ASSERT_TRUE(parseSearchAll(fx::kSearchAll, users, posts, replies));
  TEST_ASSERT_EQUAL(1, int(users.size()));
  TEST_ASSERT_TRUE(users[0].kind == SearchKind::User);
  TEST_ASSERT_EQUAL_STRING("neon", users[0].user.c_str());
  TEST_ASSERT_EQUAL_STRING("Neon Rider", users[0].title.c_str());

  TEST_ASSERT_EQUAL(2, int(posts.size()));  // the one without an id is skipped
  TEST_ASSERT_TRUE(posts[0].kind == SearchKind::Post);
  TEST_ASSERT_EQUAL_STRING("p1", posts[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("neon", posts[0].user.c_str());
  TEST_ASSERT_EQUAL_STRING("Neon nights", posts[0].title.c_str());
  TEST_ASSERT_EQUAL(5, posts[0].replies);
  TEST_ASSERT_TRUE(posts[0].createdMs > 0);
  TEST_ASSERT_TRUE(posts[1].createdMs == -1);

  TEST_ASSERT_EQUAL(1, int(replies.size()));
  TEST_ASSERT_TRUE(replies[0].kind == SearchKind::Reply);
  TEST_ASSERT_EQUAL_STRING("r1", replies[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("p1", replies[0].postId.c_str());
  TEST_ASSERT_EQUAL_STRING("bob", replies[0].user.c_str());
}

// The first non-empty line, as plain text.
void test_content_keeps_first_line() {
  std::vector<SearchHit> users, posts, replies;
  TEST_ASSERT_TRUE(parseSearchAll(fx::kSearchAll, users, posts, replies));
  TEST_ASSERT_EQUAL_STRING("Bright lights", posts[0].line.c_str());
  TEST_ASSERT_EQUAL_STRING("I like neon too", replies[0].line.c_str());
}

void test_content_cut_to_line_max() {
  std::string body = R"({"data":{"posts":[{"type":"post","postId":"p","content":")" +
                     std::string(400, 'a') + R"("}]}})";
  std::vector<SearchHit> users, posts, replies;
  TEST_ASSERT_TRUE(parseSearchAll(body, users, posts, replies));
  TEST_ASSERT_EQUAL(int(kSearchLineMax), int(posts[0].line.size()));
}

void test_all_missing_group_is_empty() {
  std::vector<SearchHit> users{SearchHit{}}, posts, replies{SearchHit{}};
  TEST_ASSERT_TRUE(parseSearchAll(fx::kSearchAllNoReplies, users, posts, replies));
  TEST_ASSERT_EQUAL(0, int(users.size()));
  TEST_ASSERT_EQUAL(1, int(posts.size()));
  TEST_ASSERT_EQUAL(0, int(replies.size()));
}

void test_page_parse_and_cursor() {
  std::vector<SearchHit> hits;
  std::string cur;
  TEST_ASSERT_TRUE(parseSearchPage(fx::kSearchPage, hits, cur));
  TEST_ASSERT_EQUAL(2, int(hits.size()));
  TEST_ASSERT_TRUE(hits[0].kind == SearchKind::Reply);
  TEST_ASSERT_EQUAL_STRING("p9", hits[1].postId.c_str());
  TEST_ASSERT_EQUAL_STRING("1", cur.c_str());

  TEST_ASSERT_TRUE(parseSearchPage(fx::kSearchPageEnd, hits, cur));
  TEST_ASSERT_EQUAL(1, int(hits.size()));
  TEST_ASSERT_TRUE(hits[0].kind == SearchKind::User);
  TEST_ASSERT_EQUAL_STRING("pixel", hits[0].user.c_str());
  TEST_ASSERT_EQUAL_STRING("", cur.c_str());
}

void test_bad_bodies_rejected() {
  std::vector<SearchHit> users, posts, replies, hits;
  std::string cur;
  TEST_ASSERT_FALSE(parseSearchAll("not json", users, posts, replies));
  TEST_ASSERT_FALSE(parseSearchAll(R"({"data":[]})", users, posts, replies));
  TEST_ASSERT_FALSE(parseSearchPage("{", hits, cur));
  TEST_ASSERT_FALSE(parseSearchPage(R"({"data":{}})", hits, cur));
}

void test_query_trimmed_and_checked() {
  TEST_ASSERT_EQUAL_STRING("neon city", searchQuery("  neon city \t").c_str());
  TEST_ASSERT_EQUAL_STRING("", searchQuery("   ").c_str());
  TEST_ASSERT_FALSE(searchQueryOk(""));
  TEST_ASSERT_TRUE(searchQueryOk("a"));
  TEST_ASSERT_TRUE(searchQueryOk(std::string(512, 'a')));
  TEST_ASSERT_FALSE(searchQueryOk(std::string(513, 'a')));
  // Characters, not bytes: 512 two-byte characters fit.
  std::string wide;
  for (int i = 0; i < 512; ++i) wide += "\xC3\xA9";
  TEST_ASSERT_TRUE(searchQueryOk(wide));
}

void test_path_encodes_query() {
  TEST_ASSERT_EQUAL_STRING("/v1/search?q=a%26b%20c%23d&type=all", searchPath("a&b c#d", "all", 3).c_str());
  TEST_ASSERT_EQUAL_STRING("/v1/search?q=caf%C3%A9&type=posts&page=2", searchPath("caf\xC3\xA9", "posts", 2).c_str());
  TEST_ASSERT_EQUAL_STRING("users", searchType(SearchKind::User));
  TEST_ASSERT_EQUAL_STRING("posts", searchType(SearchKind::Post));
  TEST_ASSERT_EQUAL_STRING("replies", searchType(SearchKind::Reply));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_groups_parse);
  RUN_TEST(test_content_keeps_first_line);
  RUN_TEST(test_content_cut_to_line_max);
  RUN_TEST(test_all_missing_group_is_empty);
  RUN_TEST(test_page_parse_and_cursor);
  RUN_TEST(test_bad_bodies_rejected);
  RUN_TEST(test_query_trimmed_and_checked);
  RUN_TEST(test_path_encodes_query);
  return UNITY_END();
}
