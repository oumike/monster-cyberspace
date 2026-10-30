#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/guild_fixtures.h"
#include "guild/guild_parse.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_guild_page() {
  std::vector<Guild> v;
  std::string cursor;
  TEST_ASSERT_TRUE(parseGuildPage(fx::kGuildPage, v, cursor));
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("xyz", cursor.c_str());
  TEST_ASSERT_EQUAL_STRING("g1", v[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("night-owls", v[0].slug.c_str());
  TEST_ASSERT_EQUAL_STRING("Night Owls", v[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("🦉", v[0].icon.c_str());
  TEST_ASSERT_EQUAL_STRING("someone", v[0].founder.c_str());
  TEST_ASSERT_EQUAL_STRING("We post after dark.", v[0].bio.c_str());
  TEST_ASSERT_EQUAL_STRING("https://example.org", v[0].link.c_str());
  TEST_ASSERT_EQUAL_STRING("our site", v[0].linkText.c_str());
  TEST_ASSERT_EQUAL(42, v[0].members);
  TEST_ASSERT_EQUAL(7, v[0].apprentices);
  TEST_ASSERT_EQUAL_INT64(1774606321516LL, v[0].createdMs);
  TEST_ASSERT_EQUAL(0, v[1].apprentices);
  TEST_ASSERT_EQUAL_STRING("", v[1].link.c_str());
  TEST_ASSERT_TRUE(parseGuildPage(fx::kGuildPageEnd, v, cursor));
  TEST_ASSERT_EQUAL_STRING("", cursor.c_str());
  TEST_ASSERT_FALSE(parseGuildPage("{\"data\":5}", v, cursor));
  TEST_ASSERT_FALSE(parseGuildPage("nope", v, cursor));
}

void test_guild_missing_counts_and_null_role() {
  Guild g;
  TEST_ASSERT_TRUE(parseGuild(fx::kGuildNotMember, g));
  TEST_ASSERT_EQUAL(0, g.apprentices);
  TEST_ASSERT_EQUAL(42, g.members);
  TEST_ASSERT_EQUAL_STRING("", g.role.c_str());
  TEST_ASSERT_EQUAL_STRING("", g.link.c_str());
}

void test_guild_role_member() {
  Guild g;
  TEST_ASSERT_TRUE(parseGuild(fx::kGuildMember, g));
  TEST_ASSERT_EQUAL_STRING("member", g.role.c_str());
  TEST_ASSERT_EQUAL(7, g.apprentices);
}

void test_my_guilds_six() {
  std::vector<MyGuild> v;
  TEST_ASSERT_TRUE(parseMyGuilds(fx::kMyGuilds, v));
  TEST_ASSERT_EQUAL(6, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("night-owls", v[0].slug.c_str());
  TEST_ASSERT_EQUAL_STRING("Night Owls", v[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("member", v[0].role.c_str());
  for (int i = 1; i < 6; ++i) TEST_ASSERT_EQUAL_STRING("apprentice", v[i].role.c_str());
  TEST_ASSERT_EQUAL_STRING("e", v[5].slug.c_str());
}

void test_members_page() {
  std::vector<Member> v;
  std::string cursor;
  TEST_ASSERT_TRUE(parseMembers(fx::kGuildMembersEnd, v, cursor));
  TEST_ASSERT_EQUAL(1, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("guildId_uid", v[0].id.c_str());
  TEST_ASSERT_EQUAL_STRING("someone", v[0].username.c_str());
  TEST_ASSERT_EQUAL_STRING("member", v[0].role.c_str());
  TEST_ASSERT_EQUAL_INT64(1774606321516LL, v[0].joinedMs);
  TEST_ASSERT_EQUAL_STRING("", cursor.c_str());
  TEST_ASSERT_TRUE(parseMembers(fx::kGuildMembers, v, cursor));
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("g1_u2", cursor.c_str());
}

void test_join_role() {
  std::string role;
  TEST_ASSERT_TRUE(parseGuildRole(fx::kGuildJoin, role));
  TEST_ASSERT_EQUAL_STRING("apprentice", role.c_str());
  TEST_ASSERT_FALSE(parseGuildRole("{\"data\":", role));
  TEST_ASSERT_FALSE(parseGuildRole("{\"data\":{\"guildId\":\"g\"}}", role));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_guild_page);
  RUN_TEST(test_guild_missing_counts_and_null_role);
  RUN_TEST(test_guild_role_member);
  RUN_TEST(test_my_guilds_six);
  RUN_TEST(test_members_page);
  RUN_TEST(test_join_role);
  return UNITY_END();
}
