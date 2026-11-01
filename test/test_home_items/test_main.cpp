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
  expectItems({HomeItem::Unread, HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail, HomeItem::Feed,
               HomeItem::Guilds, HomeItem::Settings},
              homeItems(f));
}

void test_rooms_only() {
  Features f{false, false, false, false, false};
  expectItems({HomeItem::Messages, HomeItem::Rooms, HomeItem::Settings}, homeItems(f));
}

void test_unread_off_only_drops_unread() {
  Features f{true, true, true, true, false};
  expectItems({HomeItem::Messages, HomeItem::Rooms, HomeItem::Cmail, HomeItem::Feed, HomeItem::Guilds,
               HomeItem::Settings},
              homeItems(f));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_features_keeps_todays_order);
  RUN_TEST(test_rooms_only);
  RUN_TEST(test_unread_off_only_drops_unread);
  return UNITY_END();
}
