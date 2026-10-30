#include <unity.h>

#include <string>
#include <vector>

#include "guild/guild_store.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static Guild G(const std::string& id, int members = 0) {
  Guild g;
  g.id = id;
  g.slug = id;
  g.name = "N" + id;
  g.members = members;
  return g;
}
static MyGuild M(const std::string& slug) {
  MyGuild m;
  m.slug = slug;
  m.name = "Mine " + slug;
  m.icon = "i";
  m.role = "member";
  return m;
}
static std::string ids(const std::vector<Guild>& v) {
  std::string s;
  for (const auto& g : v) s += g.id;
  return s;
}

void test_pinned_removed_from_others() {
  GuildStore s;
  s.setBrowse({G("a"), G("b", 9), G("c")}, "", true);
  s.setMine({M("b")});
  TEST_ASSERT_EQUAL_STRING("ac", ids(s.others()).c_str());
  auto p = s.pinned();
  TEST_ASSERT_EQUAL(1, (int)p.size());
  TEST_ASSERT_EQUAL_STRING("b", p[0].slug.c_str());
  TEST_ASSERT_EQUAL(9, p[0].members);
}

void test_mine_after_browse() {
  GuildStore s;
  s.setBrowse({G("a"), G("b"), G("c")}, "", true);
  s.setMine({M("c")});
  TEST_ASSERT_EQUAL_STRING("ab", ids(s.others()).c_str());
}

void test_left_guild_returns_to_browse() {
  GuildStore s;
  s.setBrowse({G("a"), G("b"), G("c")}, "", true);
  s.setMine({M("b")});
  s.setMine({});
  TEST_ASSERT_EQUAL_STRING("abc", ids(s.others()).c_str());
  TEST_ASSERT_TRUE(s.mineLoaded());
}

void test_pinned_unresolved_uses_my_guild() {
  GuildStore s;
  s.setBrowse({G("a")}, "", true);
  s.setMine({M("z")});
  auto p = s.pinned();
  TEST_ASSERT_EQUAL(1, (int)p.size());
  TEST_ASSERT_EQUAL_STRING("Mine z", p[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("member", p[0].role.c_str());
  TEST_ASSERT_EQUAL(0, p[0].members);
}

void test_append_dedupes() {
  GuildStore s;
  s.setBrowse({G("a"), G("b")}, "x", true);
  s.setBrowse({G("b"), G("c")}, "y", false);
  TEST_ASSERT_EQUAL_STRING("abc", ids(s.others()).c_str());
  TEST_ASSERT_EQUAL_STRING("y", s.browseCursor().c_str());
  TEST_ASSERT_FALSE(s.browseEnd());
}

void test_browse_cap_ends_list() {
  GuildStore s;
  std::vector<Guild> p1, p2;
  for (int i = 0; i < 199; i++) p1.push_back(G("g" + std::to_string(i)));
  for (int i = 0; i < 5; i++) p2.push_back(G("h" + std::to_string(i)));
  s.setBrowse(p1, "c1", true);
  s.setBrowse(p2, "c2", false);
  TEST_ASSERT_EQUAL(200, (int)s.others().size());
  TEST_ASSERT_EQUAL_STRING("", s.browseCursor().c_str());
  TEST_ASSERT_TRUE(s.browseEnd());
}

void test_bio_cut() {
  Guild g = G("a");
  for (int i = 0; i < 200; i++) g.bio += "é";
  GuildStore s;
  s.setBrowse({g}, "", true);
  const std::string& b = s.others()[0].bio;
  TEST_ASSERT_TRUE(b.size() <= kGuildBioBytes);
  TEST_ASSERT_EQUAL(0, (int)(b.size() % 2));
  TEST_ASSERT_TRUE(u32ToUtf8(utf8ToU32(b)) == b);
  s.openGuild(g);
  TEST_ASSERT_TRUE(s.guild()->bio.size() <= kGuildBioBytes);
  TEST_ASSERT_EQUAL(0, (int)(s.guild()->bio.size() % 2));
}

static Member Mb(int i) {
  Member m;
  m.id = "m" + std::to_string(i);
  return m;
}

void test_members_cap_and_first() {
  GuildStore s;
  s.openGuild(G("a"));
  s.appendMembers({Mb(1), Mb(2)}, "c", true);
  s.appendMembers({Mb(3)}, "d", false);
  TEST_ASSERT_EQUAL(3, (int)s.members().size());
  s.appendMembers({Mb(9)}, "e", true);
  TEST_ASSERT_EQUAL(1, (int)s.members().size());
  TEST_ASSERT_EQUAL_STRING("e", s.membersCursor().c_str());
  std::vector<Member> big;
  for (int i = 0; i < 250; i++) big.push_back(Mb(i));
  s.appendMembers(big, "f", false);
  TEST_ASSERT_EQUAL(200, (int)s.members().size());
  TEST_ASSERT_EQUAL_STRING("", s.membersCursor().c_str());
}

void test_browse_loaded() {
  GuildStore s;
  TEST_ASSERT_FALSE(s.browseLoaded());
  s.setBrowse({G("a")}, "x", true);
  TEST_ASSERT_TRUE(s.browseLoaded());
  s.clear();
  TEST_ASSERT_FALSE(s.browseLoaded());
}

void test_clear() {
  GuildStore s;
  s.setMine({M("a")});
  s.setBrowse({G("a")}, "", true);
  s.openGuild(G("a"));
  s.appendMembers({Mb(1)}, "c", true);
  s.clear();
  TEST_ASSERT_FALSE(s.mineLoaded());
  TEST_ASSERT_TRUE(s.mine().empty());
  TEST_ASSERT_TRUE(s.others().empty());
  TEST_ASSERT_TRUE(s.members().empty());
  TEST_ASSERT_NULL(s.guild());
  TEST_ASSERT_FALSE(s.browseEnd());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pinned_removed_from_others);
  RUN_TEST(test_mine_after_browse);
  RUN_TEST(test_left_guild_returns_to_browse);
  RUN_TEST(test_pinned_unresolved_uses_my_guild);
  RUN_TEST(test_append_dedupes);
  RUN_TEST(test_browse_cap_ends_list);
  RUN_TEST(test_bio_cut);
  RUN_TEST(test_members_cap_and_first);
  RUN_TEST(test_browse_loaded);
  RUN_TEST(test_clear);
  return UNITY_END();
}
