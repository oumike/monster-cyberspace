#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "chat/room_read.h"
#include "cs/api_parse.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
RoomInfo room(const char* slug, int64_t last) {
  RoomInfo r;
  r.slug = slug;
  r.lastMessageAt = last;
  return r;
}
}  // namespace

void test_never_read_room_with_messages_is_unread() {
  std::map<std::string, int64_t> readAt;
  TEST_ASSERT_TRUE(roomUnread(room("a", 5), readAt));
}

void test_empty_room_is_read() {
  std::map<std::string, int64_t> readAt;
  TEST_ASSERT_FALSE(roomUnread(room("a", 0), readAt));
}

void test_read_up_to_the_last_message() {
  std::map<std::string, int64_t> readAt{{"a", 5}};
  TEST_ASSERT_FALSE(roomUnread(room("a", 5), readAt));
  TEST_ASSERT_TRUE(roomUnread(room("a", 6), readAt));
}

void test_unread_rooms_in_list_order() {
  std::map<std::string, int64_t> readAt{{"b", 9}, {"c", 1}};
  std::vector<RoomInfo> rooms{room("a", 3), room("b", 9), room("c", 4), room("d", 0)};
  std::vector<std::string> out = unreadRooms(rooms, readAt);
  TEST_ASSERT_EQUAL(2, out.size());
  TEST_ASSERT_EQUAL_STRING("a", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("c", out[1].c_str());
  TEST_ASSERT_TRUE(unreadRooms({}, readAt).empty());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_never_read_room_with_messages_is_unread);
  RUN_TEST(test_empty_room_is_read);
  RUN_TEST(test_read_up_to_the_last_message);
  RUN_TEST(test_unread_rooms_in_list_order);
  return UNITY_END();
}
