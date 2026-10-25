#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/api_fixtures.h"
#include "cs/api_parse.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_error_envelope() {
  ApiError e = parseError(429, fx::kErrRateLimited);
  TEST_ASSERT_EQUAL(429, e.http);
  TEST_ASSERT_FALSE(e.ok());
  TEST_ASSERT_EQUAL_STRING("RATE_LIMITED", e.code.c_str());
  TEST_ASSERT_EQUAL_STRING("Too many requests", e.message.c_str());
  e = parseError(403, fx::kErrNotVerified);
  TEST_ASSERT_EQUAL_STRING("EMAIL_NOT_VERIFIED", e.code.c_str());
  TEST_ASSERT_EQUAL_STRING("Verify your email address", e.message.c_str());
  TEST_ASSERT_TRUE(parseError(201, "{\"data\":{}}").ok());
}

void test_error_non_json() {
  ApiError e = parseError(502, fx::kErr502Html);
  TEST_ASSERT_EQUAL_STRING("HTTP_502", e.code.c_str());
  TEST_ASSERT_EQUAL_STRING("Server error", e.message.c_str());
}

void test_login_tokens() {
  Tokens t;
  TEST_ASSERT_TRUE(parseTokens(fx::kLogin, t));
  TEST_ASSERT_EQUAL_STRING("eyJhbGciOiJSUzI1NiJ9.e30.sig", t.idToken.c_str());
  TEST_ASSERT_EQUAL_STRING("AMf-refresh", t.refreshToken.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "https://cyberspace-cyberspace-default-rtdb.europe-west1.firebasedatabase.app",
      t.rtdbUrl.c_str());
}

void test_refresh_keeps_refresh_token() {
  Tokens t;
  t.refreshToken = "AMf-old";
  TEST_ASSERT_TRUE(parseTokens(fx::kRefresh, t));
  TEST_ASSERT_EQUAL_STRING("eyJnew", t.idToken.c_str());
  TEST_ASSERT_EQUAL_STRING("AMf-old", t.refreshToken.c_str());
  Tokens bad;
  TEST_ASSERT_FALSE(parseTokens("{\"data\":{}}", bad));
  TEST_ASSERT_FALSE(parseTokens("nope", bad));
}

void test_rooms() {
  std::vector<RoomInfo> r;
  TEST_ASSERT_TRUE(parseRooms(fx::kRooms, r));
  TEST_ASSERT_EQUAL(3, r.size());
  TEST_ASSERT_EQUAL_STRING("general", r[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("general", r[0].slug.c_str());
  TEST_ASSERT_EQUAL_STRING("General", r[0].name.c_str());
  TEST_ASSERT_TRUE(r[0].lastMessageAt == 1719700000000LL);
  TEST_ASSERT_EQUAL(0, r[0].sortOrder);
  TEST_ASSERT_EQUAL(12, r[0].onlineCount);
  TEST_ASSERT_EQUAL_STRING("Quiet Room", r[2].name.c_str());
  TEST_ASSERT_TRUE(r[2].lastMessageAt == 0);
  TEST_ASSERT_EQUAL(2, r[2].sortOrder);
}

void test_history() {
  std::vector<CircMessage> m;
  int64_t cursor = 0;
  TEST_ASSERT_TRUE(parseHistory(fx::kHistory, m, cursor));
  TEST_ASSERT_TRUE(cursor == 1719700000000LL);
  TEST_ASSERT_EQUAL(3, m.size());
  TEST_ASSERT_EQUAL_STRING("-Nh1", m[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("u1", m[0].userId.c_str());
  TEST_ASSERT_EQUAL_STRING("first", m[0].content.c_str());
  TEST_ASSERT_TRUE(m[1].isChatAdmin);
  TEST_ASSERT_TRUE(m[1].isAction);

  TEST_ASSERT_TRUE(parseHistory(fx::kHistoryStringCursor, m, cursor));
  TEST_ASSERT_EQUAL(0, m.size());
  TEST_ASSERT_TRUE(cursor == 1719600000000LL);
}

void test_history_null_cursor() {
  std::vector<CircMessage> m;
  int64_t cursor = 0;
  TEST_ASSERT_TRUE(parseHistory(fx::kHistoryNullCursor, m, cursor));
  TEST_ASSERT_EQUAL(-1, cursor);
  TEST_ASSERT_EQUAL(1, m.size());
}

void test_message_empty_content_with_image() {
  std::vector<CircMessage> m;
  int64_t cursor = 0;
  TEST_ASSERT_TRUE(parseHistory(fx::kHistory, m, cursor));
  TEST_ASSERT_EQUAL_STRING("", m[2].content.c_str());
  TEST_ASSERT_EQUAL_STRING("https://bunker.cyberspace.online/i.png", m[2].imageUrl.c_str());
}

void test_users() {
  std::vector<PresenceEntry> u;
  TEST_ASSERT_TRUE(parseUsers(fx::kUsers, u));
  TEST_ASSERT_EQUAL(2, u.size());
  TEST_ASSERT_EQUAL_STRING("u2", u[0].userId.c_str());
  TEST_ASSERT_EQUAL_STRING("genghis_khan", u[0].username.c_str());
  TEST_ASSERT_TRUE(u[0].online);
  TEST_ASSERT_TRUE(u[0].isChatAdmin);
  TEST_ASSERT_TRUE(u[0].lastSeen == 1719700005000LL);
  TEST_ASSERT_TRUE(u[0].lastActivity == 1719690000000LL);
  TEST_ASSERT_TRUE(u[1].online);
  TEST_ASSERT_EQUAL(-1, u[1].lastActivity);
}

void test_presence_ack() {
  PresenceCadence c;
  c.heartbeatMs = c.staleAfterMs = c.idleAfterMs = 1;
  TEST_ASSERT_TRUE(parsePresenceAck(fx::kPresenceAck, c));
  TEST_ASSERT_EQUAL(30000, c.heartbeatMs);
  TEST_ASSERT_EQUAL(180000, c.staleAfterMs);
  TEST_ASSERT_EQUAL(600000, c.idleAfterMs);
}

void test_send_result_and_reply() {
  std::string id, reply;
  TEST_ASSERT_TRUE(parseSendResult(fx::kSendResult, id, reply));
  TEST_ASSERT_EQUAL_STRING("-Nsent", id.c_str());
  TEST_ASSERT_EQUAL_STRING("", reply.c_str());
  TEST_ASSERT_TRUE(parseSendResult(fx::kHelpReply, id, reply));
  TEST_ASSERT_EQUAL_STRING("", id.c_str());
  TEST_ASSERT_EQUAL_STRING("Commands: /me /dice /8ball ...", reply.c_str());
}

void test_mentions() {
  std::vector<Mention> m;
  TEST_ASSERT_TRUE(parseMentions(fx::kMentions, m));
  TEST_ASSERT_EQUAL(2, m.size());
  TEST_ASSERT_EQUAL_STRING("n1", m[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("general", m[0].roomSlug.c_str());
  TEST_ASSERT_EQUAL_STRING("General", m[0].roomName.c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte", m[0].actorUsername.c_str());
  TEST_ASSERT_EQUAL_STRING("hey @oumike", m[0].messageContent.c_str());
  TEST_ASSERT_EQUAL_STRING("n2", m[1].id.c_str());
  TEST_ASSERT_EQUAL_STRING("", m[1].roomSlug.c_str());
  TEST_ASSERT_EQUAL_STRING("", m[1].actorUsername.c_str());
}

void test_unread_count() {
  int count = 0;
  bool exact = true;
  TEST_ASSERT_TRUE(parseUnreadCount(fx::kUnread, count, exact));
  TEST_ASSERT_EQUAL(100, count);
  TEST_ASSERT_FALSE(exact);
  TEST_ASSERT_FALSE(parseUnreadCount("<html>", count, exact));
}

void test_muted_users() {
  MutedUsers m;
  // Room keys map to arrays or {name: true} objects; false entries are not muted.
  TEST_ASSERT_TRUE(parseMutedUsers(
      R"({"data":{"mutedUsersByRoom":{"r1":["Zed","moth"],"linux":{"kiwi":true,"rain":false}}}})", m));
  TEST_ASSERT_EQUAL(2, m.size());
  TEST_ASSERT_EQUAL(1, m["r1"].count("zed"));
  TEST_ASSERT_EQUAL(1, m["r1"].count("moth"));
  TEST_ASSERT_EQUAL(1, m["linux"].count("kiwi"));
  TEST_ASSERT_EQUAL(0, m["linux"].count("rain"));
  // Missing field: an empty map, still a success.
  TEST_ASSERT_TRUE(parseMutedUsers(R"({"data":{"filterNSFW":true}})", m));
  TEST_ASSERT_EQUAL(0, m.size());
  TEST_ASSERT_FALSE(parseMutedUsers("<html>", m));
}

void test_conversations() {
  std::vector<ConversationInfo> v;
  TEST_ASSERT_TRUE(parseConversations(fx::kConversations, v));
  TEST_ASSERT_EQUAL(3, int(v.size()));
  TEST_ASSERT_EQUAL_STRING("c-rain", v[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("rain", v[0].partner.c_str());
  TEST_ASSERT_EQUAL_STRING("u-rain", v[0].partnerId.c_str());
  TEST_ASSERT_EQUAL_STRING("you around?", v[0].lastMessage.c_str());
  TEST_ASSERT_EQUAL_INT64(1719700050000LL, v[0].lastMessageAt);
  TEST_ASSERT_EQUAL(2, v[0].unread);
  TEST_ASSERT_FALSE(v[0].partnerDeleted);
  TEST_ASSERT_EQUAL(0, v[1].unread);
  TEST_ASSERT_TRUE(v[2].partnerDeleted);
  TEST_ASSERT_EQUAL_INT64(0, v[2].lastMessageAt);
  TEST_ASSERT_FALSE(parseConversations("{\"data\":{}}", v));
}

void test_start_conversation() {
  ConversationInfo c;
  TEST_ASSERT_TRUE(parseStartConversation(fx::kStartConversation, c));
  TEST_ASSERT_EQUAL_STRING("c-kiwi", c.id.c_str());
  TEST_ASSERT_EQUAL_STRING("kiwi", c.partner.c_str());
  TEST_ASSERT_EQUAL_STRING("u-kiwi", c.partnerId.c_str());
  TEST_ASSERT_FALSE(parseStartConversation("{\"data\":{\"otherUser\":{}}}", c));
}

void test_typing_ack() {
  TypingCadence t;
  TEST_ASSERT_TRUE(parseTypingAck(fx::kTypingAck, t));
  TEST_ASSERT_EQUAL_INT64(2500, t.heartbeatMs);
  TEST_ASSERT_EQUAL_INT64(8000, t.staleAfterMs);
  TypingCadence d;
  TEST_ASSERT_FALSE(parseTypingAck("nope", d));
  TEST_ASSERT_EQUAL_INT64(3000, d.heartbeatMs);
}

void test_cmail_history_sender_aliases() {
  std::vector<CircMessage> v;
  int64_t cursor = 0;
  TEST_ASSERT_TRUE(parseHistory(fx::kCmailHistory, v, cursor));
  TEST_ASSERT_EQUAL(2, int(v.size()));
  TEST_ASSERT_EQUAL_STRING("rain", v[0].username.c_str());
  TEST_ASSERT_EQUAL_STRING("u-rain", v[0].userId.c_str());
  TEST_ASSERT_EQUAL_STRING("oumike", v[1].username.c_str());
  TEST_ASSERT_EQUAL_INT64(1719700000000LL, cursor);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_error_envelope);
  RUN_TEST(test_error_non_json);
  RUN_TEST(test_login_tokens);
  RUN_TEST(test_refresh_keeps_refresh_token);
  RUN_TEST(test_rooms);
  RUN_TEST(test_history);
  RUN_TEST(test_history_null_cursor);
  RUN_TEST(test_message_empty_content_with_image);
  RUN_TEST(test_users);
  RUN_TEST(test_presence_ack);
  RUN_TEST(test_send_result_and_reply);
  RUN_TEST(test_mentions);
  RUN_TEST(test_unread_count);
  RUN_TEST(test_muted_users);
  RUN_TEST(test_conversations);
  RUN_TEST(test_start_conversation);
  RUN_TEST(test_typing_ack);
  RUN_TEST(test_cmail_history_sender_aliases);
  return UNITY_END();
}
