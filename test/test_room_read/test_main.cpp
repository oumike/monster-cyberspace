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

void test_read_marks_round_trip() {
  std::map<std::string, int64_t> marks{{"general", 1730000000123LL}, {"linux", 5}};
  std::string user;
  std::map<std::string, int64_t> out;
  TEST_ASSERT_TRUE(decodeReadMarks(encodeReadMarks("mike", marks, 4000), user, out));
  TEST_ASSERT_EQUAL_STRING("mike", user.c_str());
  TEST_ASSERT_TRUE(out == marks);
}

void test_read_marks_reject_garbage() {
  std::string user;
  std::map<std::string, int64_t> out;
  TEST_ASSERT_FALSE(decodeReadMarks("", user, out));
  TEST_ASSERT_TRUE(decodeReadMarks("mike\nok 7\nbad\nx notanumber\n y 3\n", user, out));
  TEST_ASSERT_EQUAL(1, out.size());
  TEST_ASSERT_EQUAL_INT64(7, out["ok"]);
}

void test_read_marks_fit_the_limit() {
  std::map<std::string, int64_t> marks;
  for (int i = 0; i < 100; ++i) marks["room" + std::to_string(i)] = 1730000000000LL + i;
  std::string text = encodeReadMarks("mike", marks, 200);
  TEST_ASSERT_TRUE(text.size() <= 200);
  std::string user;
  std::map<std::string, int64_t> out;
  TEST_ASSERT_TRUE(decodeReadMarks(text, user, out));
  TEST_ASSERT_TRUE(out.size() > 0 && out.size() < marks.size());
  for (const auto& kv : out) TEST_ASSERT_EQUAL_INT64(marks[kv.first], kv.second);
}

void test_room_read_marks_keep_only_rooms() {
  std::map<std::string, int64_t> readAt{{"a", 3}, {"conv-id", 9}, {"c", 0}};
  std::vector<RoomInfo> rooms{room("a", 3), room("b", 4), room("c", 1)};
  std::map<std::string, int64_t> out = roomReadMarks(rooms, readAt);
  TEST_ASSERT_EQUAL(2, out.size());
  TEST_ASSERT_EQUAL_INT64(3, out["a"]);
  TEST_ASSERT_EQUAL_INT64(0, out["c"]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_never_read_room_with_messages_is_unread);
  RUN_TEST(test_empty_room_is_read);
  RUN_TEST(test_read_up_to_the_last_message);
  RUN_TEST(test_unread_rooms_in_list_order);
  RUN_TEST(test_read_marks_round_trip);
  RUN_TEST(test_read_marks_reject_garbage);
  RUN_TEST(test_read_marks_fit_the_limit);
  RUN_TEST(test_room_read_marks_keep_only_rooms);
  return UNITY_END();
}
