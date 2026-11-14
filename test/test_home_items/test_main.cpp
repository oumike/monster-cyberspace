#include <unity.h>

#include <vector>

#include "ui/home_items.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static void expectItems(const std::vector<HomeItem>& want, const std::vector<HomeItem>& got) {
  TEST_ASSERT_EQUAL(int(want.size()), int(got.size()));
  for (size_t i = 0; i < want.size(); ++i) TEST_ASSERT_EQUAL(int(want[i]), int(got[i]));
}

void test_all_features_keeps_todays_order() {
  Features f{true, true, true, true, true};
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail,
               HomeItem::Feed, HomeItem::Following, HomeItem::Notes, HomeItem::Guilds, HomeItem::Programs,
               HomeItem::Profile, HomeItem::Search, HomeItem::Settings},
              homeItems(f));
}

// Search, just before Settings, only with Search and People built.
void test_search_before_settings() {
  Features f{true, true, true, true, true};
  f.search = false;
  for (HomeItem h : homeItems(f)) TEST_ASSERT_TRUE(h != HomeItem::Search);
  Features noPeople{true, true, true, false, true};
  for (HomeItem h : homeItems(noPeople)) TEST_ASSERT_TRUE(h != HomeItem::Search);
  const auto all = homeItems(Features{true, true, true, true, true});
  TEST_ASSERT_EQUAL(int(HomeItem::Search), int(all[all.size() - 2]));
}

// Programs, after Guilds, only with Programs (and the Feed) built.
void test_programs_needs_feed() {
  Features f{true, true, true, true, true};
  f.programs = false;
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail,
               HomeItem::Feed, HomeItem::Following, HomeItem::Notes, HomeItem::Guilds, HomeItem::Profile,
               HomeItem::Search, HomeItem::Settings},
              homeItems(f));
  Features noFeed{true, false, true, true, true};
  for (HomeItem h : homeItems(noFeed)) TEST_ASSERT_TRUE(h != HomeItem::Programs);
}

// Notes, after Following, only with Notes (and the Feed) built.
void test_notes_off_drops_notes() {
  Features f{true, true, true, true, true};
  f.notes = false;
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail,
               HomeItem::Feed, HomeItem::Following, HomeItem::Guilds, HomeItem::Programs, HomeItem::Profile,
               HomeItem::Search, HomeItem::Settings},
              homeItems(f));
}

void test_span_first_with_notes() {
  TEST_ASSERT_FALSE(homeSpanFirst(homeItems(Features{true, true, true, true, true})));  // 12
  Features f{true, true, true, true, true};
  f.notes = false;
  TEST_ASSERT_TRUE(homeSpanFirst(homeItems(f)));  // 11: 1 + 5x2
  f.programs = false;
  TEST_ASSERT_FALSE(homeSpanFirst(homeItems(f)));  // 10
}

// Following, after the Feed, needs both the Feed and People.
void test_following_needs_feed_and_people() {
  Features noFeed{true, false, true, true, true};
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail,
               HomeItem::Guilds, HomeItem::Profile, HomeItem::Search, HomeItem::Settings},
              homeItems(noFeed));
}

// Your own profile, before Settings, only with People built.
void test_profile_needs_people() {
  Features f{true, true, true, false, true};
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail, HomeItem::Feed,
               HomeItem::Notes, HomeItem::Guilds, HomeItem::Programs, HomeItem::Settings},
              homeItems(f));
  Features p{false, false, false, true, false};
  expectItems({HomeItem::Messages, HomeItem::Rooms, HomeItem::Profile, HomeItem::Search, HomeItem::Settings},
              homeItems(p));
}

// Unread takes a row of its own only when the rest then fill their rows.
void test_span_first_only_with_full_rows() {
  Features nine{true, true, true, true, true};
  nine.notes = false;
  nine.programs = false;
  nine.search = false;
  TEST_ASSERT_TRUE(homeSpanFirst(homeItems(nine)));  // 9: 1 + 4x2
  Features eight{true, true, true, false, true};
  eight.programs = false;
  eight.search = false;
  TEST_ASSERT_FALSE(homeSpanFirst(homeItems(eight)));  // 8
  TEST_ASSERT_FALSE(homeSpanFirst(homeItems(Features{true, true, true, true, false})));  // no unread
  TEST_ASSERT_FALSE(homeSpanFirst({}));
}

void test_rooms_only() {
  Features f{false, false, false, false, false};
  expectItems({HomeItem::Messages, HomeItem::Rooms, HomeItem::Settings}, homeItems(f));
}

void test_unread_off_only_drops_unread() {
  Features f{true, true, true, true, false};
  expectItems({HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail, HomeItem::Feed,
               HomeItem::Following, HomeItem::Notes, HomeItem::Guilds, HomeItem::Programs, HomeItem::Profile,
               HomeItem::Search, HomeItem::Settings},
              homeItems(f));
}

void test_circ_off_drops_messages_and_rooms() {
  Features f{true, true, true, true, true, false, false};
  f.programs = false;
  f.search = false;
  const auto items = homeItems(f);
  expectItems({HomeItem::Unread, HomeItem::Cmail, HomeItem::Feed, HomeItem::Following, HomeItem::Guilds,
               HomeItem::Profile, HomeItem::Settings},
              items);
  TEST_ASSERT_TRUE(homeSpanFirst(items));  // 7: 1 + 3x2
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_features_keeps_todays_order);
  RUN_TEST(test_rooms_only);
  RUN_TEST(test_circ_off_drops_messages_and_rooms);
  RUN_TEST(test_unread_off_only_drops_unread);
  RUN_TEST(test_profile_needs_people);
  RUN_TEST(test_following_needs_feed_and_people);
  RUN_TEST(test_notes_off_drops_notes);
  RUN_TEST(test_programs_needs_feed);
  RUN_TEST(test_search_before_settings);
  RUN_TEST(test_span_first_with_notes);
  RUN_TEST(test_span_first_only_with_full_rows);
  return UNITY_END();
}
