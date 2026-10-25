#include <unity.h>

#include <string>
#include <vector>

#include "chat/chat_store.h"
#include "chat/mention.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static CircMessage msg(const std::string& id, int64_t ts,
                       const std::string& content = "") {
  CircMessage m;
  m.id = id;
  m.timestamp = ts;
  m.content = content;
  return m;
}

static std::string ids(const ChatStore& s) {
  std::string out;
  for (const auto& m : s.messages()) out += m.id;
  return out;
}

void test_upsert_orders() {
  ChatStore s;
  s.upsert(msg("c", 30));
  s.upsert(msg("a", 10));
  s.upsert(msg("b", 20));
  TEST_ASSERT_EQUAL_STRING("abc", ids(s).c_str());
  TEST_ASSERT_EQUAL(10, s.oldestTimestamp());
  TEST_ASSERT_EQUAL(30, s.newestTimestamp());
}

void test_upsert_dedupes() {
  ChatStore s;
  s.upsert(msg("a", 10, "first"));
  s.upsert(msg("a", 10, "second"));
  TEST_ASSERT_EQUAL(1, s.size());
  TEST_ASSERT_EQUAL_STRING("second", s.find("a")->content.c_str());
}

void test_cap_drops_oldest() {
  ChatStore s(3);
  s.upsert(msg("a", 10));
  s.upsert(msg("b", 20));
  s.upsert(msg("c", 30));
  s.upsert(msg("d", 40));
  TEST_ASSERT_EQUAL(3, s.size());
  TEST_ASSERT_NULL(s.find("a"));
  TEST_ASSERT_EQUAL(20, s.oldestTimestamp());
}

void test_remove() {
  ChatStore s;
  s.upsert(msg("a", 10));
  s.upsert(msg("b", 20));
  TEST_ASSERT_TRUE(s.remove("a"));
  TEST_ASSERT_EQUAL(1, s.size());
  TEST_ASSERT_FALSE(s.remove("zzz"));
  TEST_ASSERT_EQUAL(1, s.size());
}

void test_version_bumps() {
  ChatStore s;
  TEST_ASSERT_EQUAL(-1, s.newestTimestamp());
  TEST_ASSERT_EQUAL(-1, s.oldestTimestamp());
  uint32_t v0 = s.version();
  s.upsert(msg("a", 10));
  uint32_t v1 = s.version();
  TEST_ASSERT_TRUE(v1 > v0);
  s.remove("a");
  TEST_ASSERT_TRUE(s.version() > v1);
}

void test_replace_all_dedupes_and_sorts() {
  ChatStore s;
  s.upsert(msg("z", 5));
  s.replaceAll({msg("c", 30), msg("a", 10, "old"), msg("b", 20),
                msg("a", 10, "new")});
  TEST_ASSERT_EQUAL_STRING("abc", ids(s).c_str());
  TEST_ASSERT_NULL(s.find("z"));
  TEST_ASSERT_EQUAL_STRING("new", s.find("a")->content.c_str());
}

void test_overlap_history_and_stream() {
  ChatStore s;
  s.replaceAll({msg("a", 10), msg("b", 20, "v1"), msg("c", 30)});
  s.upsert(msg("b", 20, "v2"));
  s.upsert(msg("d", 40));
  TEST_ASSERT_EQUAL_STRING("abcd", ids(s).c_str());
  TEST_ASSERT_EQUAL_STRING("v2", s.find("b")->content.c_str());
}

void test_upsert_resorts_on_timestamp_change() {
  ChatStore s;
  s.replaceAll({msg("a", 10), msg("b", 20), msg("c", 30)});
  s.upsert(msg("a", 35));
  TEST_ASSERT_EQUAL_STRING("bca", ids(s).c_str());
}

void test_mentions() {
  TEST_ASSERT_TRUE(mentionsUser("hey @OuMike!", "oumike"));
  TEST_ASSERT_FALSE(mentionsUser("@oumikes", "oumike"));
  TEST_ASSERT_FALSE(mentionsUser("mail@oumike", "oumike"));
  TEST_ASSERT_TRUE(mentionsUser("@oumike", "oumike"));
  TEST_ASSERT_TRUE(mentionsUser("x @nope @oumike", "oumike"));
  TEST_ASSERT_FALSE(mentionsUser("@oumike", ""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_upsert_orders);
  RUN_TEST(test_upsert_dedupes);
  RUN_TEST(test_cap_drops_oldest);
  RUN_TEST(test_remove);
  RUN_TEST(test_version_bumps);
  RUN_TEST(test_replace_all_dedupes_and_sorts);
  RUN_TEST(test_overlap_history_and_stream);
  RUN_TEST(test_upsert_resorts_on_timestamp_change);
  RUN_TEST(test_mentions);
  return UNITY_END();
}
