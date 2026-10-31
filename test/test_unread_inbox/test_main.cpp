#include <unity.h>

#include <string>
#include <vector>

#include "ui/unread_inbox.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
UnreadItem item(UnreadKind k, const char* target, int64_t at, int count = 1) {
  UnreadItem it;
  it.kind = k;
  it.target = target;
  it.text = target;
  it.at = at;
  it.count = count;
  return it;
}

std::string order(const std::vector<UnreadItem>& v) {
  std::string out;
  for (const auto& it : v) out += it.target + " ";
  return out;
}
}  // namespace

void test_merges_newest_first_across_sources() {
  auto out = mergeUnread({item(UnreadKind::Cmail, "c1", 300), item(UnreadKind::Cmail, "c2", 100)},
                         {item(UnreadKind::Mention, "m1", 400), item(UnreadKind::Mention, "m2", 150)},
                         {item(UnreadKind::Feed, "f1", 200)});
  TEST_ASSERT_EQUAL_STRING("m1 c1 f1 m2 c2 ", order(out).c_str());
  TEST_ASSERT_EQUAL(int(UnreadKind::Mention), int(out[0].kind));
  TEST_ASSERT_EQUAL(int(UnreadKind::Feed), int(out[2].kind));
}

void test_ties_keep_source_order() {
  auto out = mergeUnread({item(UnreadKind::Cmail, "c1", 100)},
                         {item(UnreadKind::Mention, "m1", 100), item(UnreadKind::Mention, "m2", 100)},
                         {item(UnreadKind::Feed, "f1", 100)});
  TEST_ASSERT_EQUAL_STRING("c1 m1 m2 f1 ", order(out).c_str());
}

void test_unknown_times_go_last_in_source_order() {
  auto out = mergeUnread({item(UnreadKind::Cmail, "c0", 0), item(UnreadKind::Cmail, "c1", 50)},
                         {item(UnreadKind::Mention, "m0", 0)},
                         {item(UnreadKind::Feed, "f0", 0), item(UnreadKind::Feed, "f1", 60)});
  TEST_ASSERT_EQUAL_STRING("f1 c1 c0 m0 f0 ", order(out).c_str());
}

void test_total_counts_cmail_messages() {
  auto out = mergeUnread({item(UnreadKind::Cmail, "c1", 10, 3), item(UnreadKind::Cmail, "c2", 20, 2)},
                         {item(UnreadKind::Mention, "m1", 30)}, {item(UnreadKind::Feed, "f1", 40)});
  TEST_ASSERT_EQUAL(7, unreadTotal(out));
}

void test_empty() {
  auto out = mergeUnread({}, {}, {});
  TEST_ASSERT_EQUAL(0, out.size());
  TEST_ASSERT_EQUAL(0, unreadTotal(out));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_merges_newest_first_across_sources);
  RUN_TEST(test_ties_keep_source_order);
  RUN_TEST(test_unknown_times_go_last_in_source_order);
  RUN_TEST(test_total_counts_cmail_messages);
  RUN_TEST(test_empty);
  return UNITY_END();
}
