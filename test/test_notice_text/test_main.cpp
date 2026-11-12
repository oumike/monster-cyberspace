// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <string>

#include "cs/api_parse.h"
#include "ui/notice_text.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
Mention notice(const char* type, const char* actor, const char* target = "p1") {
  Mention m;
  m.id = "n1";
  m.type = type;
  m.actorUsername = actor;
  m.targetId = target;
  return m;
}
}  // namespace

void test_follower_and_poke_open_the_profile() {
  NoticeView v = describeNotice(notice("new_follower", "rain", ""), 80);
  TEST_ASSERT_TRUE(v.open == NoticeOpen::Profile);
  TEST_ASSERT_EQUAL_STRING("@rain followed you", v.text.c_str());
  TEST_ASSERT_EQUAL_STRING("rain", v.user.c_str());
  v = describeNotice(notice("poke", "moth", ""), 80);
  TEST_ASSERT_TRUE(v.open == NoticeOpen::Profile);
  TEST_ASSERT_EQUAL_STRING("@moth poked you", v.text.c_str());
  TEST_ASSERT_EQUAL_STRING("moth", v.user.c_str());
}

void test_bookmark_opens_the_entry() {
  Mention m = notice("bookmark", "moth", "p7");
  NoticeView v = describeNotice(m, 80);
  TEST_ASSERT_TRUE(v.open == NoticeOpen::Entry);
  TEST_ASSERT_EQUAL_STRING("@moth bookmarked your entry", v.text.c_str());
  m.targetType = "reply";
  TEST_ASSERT_EQUAL_STRING("@moth bookmarked your reply", describeNotice(m, 80).text.c_str());
}

void test_new_post_shows_the_title_else_the_text() {
  Mention m = notice("new_post_following", "kiwi", "p8");
  m.postTitle = "Moths at dusk";
  m.preview = "they came back";
  NoticeView v = describeNotice(m, 80);
  TEST_ASSERT_TRUE(v.open == NoticeOpen::Entry);
  TEST_ASSERT_EQUAL_STRING("@kiwi posted: Moths at dusk", v.text.c_str());
  m.type = "new_post_friend";
  m.postTitle.clear();
  TEST_ASSERT_EQUAL_STRING("@kiwi posted: they came back", describeNotice(m, 80).text.c_str());
  m.preview.clear();
  TEST_ASSERT_EQUAL_STRING("@kiwi posted an entry", describeNotice(m, 80).text.c_str());
}

void test_preview_is_cut_to_the_limit() {
  Mention m = notice("reply", "rain");
  m.preview = "abcdefghij";
  TEST_ASSERT_EQUAL_STRING("@rain replied: abcd", describeNotice(m, 4).text.c_str());
}

void test_replies_mentions_and_guild_threads_keep_their_text() {
  Mention m = notice("reply", "rain");
  m.preview = "nice";
  NoticeView v = describeNotice(m, 80);
  TEST_ASSERT_TRUE(v.open == NoticeOpen::Entry);
  TEST_ASSERT_EQUAL_STRING("@rain replied: nice", v.text.c_str());
  m.type = "reply_mention";
  TEST_ASSERT_EQUAL_STRING("@rain mentioned you: nice", describeNotice(m, 80).text.c_str());
  m.type = "guild_new_thread";
  m.guildName = "Night Owls";
  TEST_ASSERT_EQUAL_STRING("@rain in Night Owls: nice", describeNotice(m, 80).text.c_str());
}

void test_nothing_to_open_is_dropped() {
  // An entry notice without a target would open GET /v1/posts/.
  TEST_ASSERT_TRUE(describeNotice(notice("bookmark", "moth", ""), 80).open == NoticeOpen::None);
  TEST_ASSERT_TRUE(describeNotice(notice("reply", "rain", ""), 80).open == NoticeOpen::None);
  // No sender, or the system: no profile to open.
  TEST_ASSERT_TRUE(describeNotice(notice("new_follower", "", ""), 80).open == NoticeOpen::None);
  TEST_ASSERT_TRUE(describeNotice(notice("poke", "system", ""), 80).open == NoticeOpen::None);
  TEST_ASSERT_TRUE(describeNotice(notice("system_ban", "", ""), 80).open == NoticeOpen::None);
}

void test_social_types() {
  for (const char* t : {"new_follower", "poke", "bookmark", "new_post_following", "new_post_friend"})
    TEST_ASSERT_TRUE_MESSAGE(isSocialNotice(t), t);
  for (const char* t : {"reply", "post_mention", "guild_new_thread", "chat_mention", ""})
    TEST_ASSERT_FALSE_MESSAGE(isSocialNotice(t), t);
}

void test_filter_picks_the_types_asked_for() {
  const std::string mentions = noticeTypes(UnreadFilter::Mentions);
  const std::string all = noticeTypes(UnreadFilter::All);
  TEST_ASSERT_EQUAL_STRING(
      "chat_mention,reply,thread_reply,post_mention,reply_mention,post_cooldown,guild_new_thread",
      mentions.c_str());
  TEST_ASSERT_EQUAL_STRING((mentions + ",new_follower,poke,bookmark,new_post_following,new_post_friend").c_str(),
                           all.c_str());
}

void test_filter_names() {
  TEST_ASSERT_TRUE(nextUnreadFilter(UnreadFilter::All) == UnreadFilter::Mentions);
  TEST_ASSERT_TRUE(nextUnreadFilter(UnreadFilter::Mentions) == UnreadFilter::All);
  TEST_ASSERT_TRUE(unreadFilterFromIndex(1) == UnreadFilter::Mentions);
  TEST_ASSERT_TRUE(unreadFilterFromIndex(9) == UnreadFilter::All);
  TEST_ASSERT_EQUAL_STRING("all", unreadFilterKey(UnreadFilter::All));
  TEST_ASSERT_EQUAL_STRING("mentions", unreadFilterKey(UnreadFilter::Mentions));
  UnreadFilter f = UnreadFilter::All;
  TEST_ASSERT_TRUE(parseUnreadFilter("mentions", f));
  TEST_ASSERT_TRUE(f == UnreadFilter::Mentions);
  TEST_ASSERT_FALSE(parseUnreadFilter("some", f));
  TEST_ASSERT_TRUE(f == UnreadFilter::Mentions);
  TEST_ASSERT_TRUE(unreadFilterLabel(UnreadFilter::All) == U"All");
  TEST_ASSERT_TRUE(unreadFilterLabel(UnreadFilter::Mentions) == U"Replies & mentions");
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_follower_and_poke_open_the_profile);
  RUN_TEST(test_bookmark_opens_the_entry);
  RUN_TEST(test_new_post_shows_the_title_else_the_text);
  RUN_TEST(test_preview_is_cut_to_the_limit);
  RUN_TEST(test_replies_mentions_and_guild_threads_keep_their_text);
  RUN_TEST(test_nothing_to_open_is_dropped);
  RUN_TEST(test_social_types);
  RUN_TEST(test_filter_picks_the_types_asked_for);
  RUN_TEST(test_filter_names);
  return UNITY_END();
}
