#include <unity.h>

#include <string>
#include <vector>

#include "../fixtures/people_fixtures.h"
#include "feed/feed_parse.h"
#include "people/follow_index.h"
#include "people/people_parse.h"
#include "people/profile_edit.h"
#include <ArduinoJson.h>
#include "ui/profile_layout.h"
#include "ui/widgets/input_line.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {

bool hasLine(const std::u32string& t, const char* l) {
  std::u32string n = U"\n" + t + U"\n";
  return n.find(U"\n" + utf8ToU32(l) + U"\n") != std::u32string::npos;
}

}  // namespace

void test_profile_full() {
  Profile p;
  TEST_ASSERT_TRUE(parseProfile(fx::kProfileFull, p));
  TEST_ASSERT_EQUAL_STRING("u42", p.userId.c_str());
  TEST_ASSERT_EQUAL_STRING("ann", p.username.c_str());
  TEST_ASSERT_EQUAL_STRING("Ann Example", p.displayName.c_str());
  TEST_ASSERT_EQUAL_STRING("I like **night** walks.", p.bio.c_str());
  TEST_ASSERT_EQUAL(117, p.serial);
  TEST_ASSERT_TRUE(p.supporter);
  TEST_ASSERT_EQUAL(40, p.followers);
  TEST_ASSERT_EQUAL(12, p.following);
  TEST_ASSERT_EQUAL(5, p.publicPosts);
  TEST_ASSERT_EQUAL_STRING("https://ann.example", p.websiteUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("my site", p.websiteName.c_str());
  TEST_ASSERT_EQUAL_STRING("London, UK", p.locationName.c_str());
  TEST_ASSERT_EQUAL_STRING("night-owls", p.guildSlug.c_str());
  TEST_ASSERT_EQUAL_STRING("Night Owls", p.guildName.c_str());
  TEST_ASSERT_EQUAL_INT64(parseIsoMs("2026-03-27T10:12:01.516Z"), p.createdMs);
  TEST_ASSERT_TRUE(p.createdMs > 0);
  TEST_ASSERT_EQUAL_INT64(parseIsoMs("2026-10-31T09:00:00.000Z"), p.lastActiveMs);
}

void test_profile_missing_fields() {
  Profile p;
  p.followers = 99;  // a reused Profile starts over
  TEST_ASSERT_TRUE(parseProfile(fx::kProfileBare, p));
  TEST_ASSERT_EQUAL_STRING("bob", p.username.c_str());
  TEST_ASSERT_EQUAL(-1, p.followers);
  TEST_ASSERT_EQUAL(-1, p.following);
  TEST_ASSERT_EQUAL(-1, p.serial);
  TEST_ASSERT_EQUAL_INT64(-1, p.createdMs);
  TEST_ASSERT_EQUAL_INT64(-1, p.lastActiveMs);
  TEST_ASSERT_FALSE(p.supporter);
  TEST_ASSERT_FALSE(parseProfile(fx::kProfileNoName, p));
  TEST_ASSERT_FALSE(parseProfile("{\"data\":[]}", p));
  TEST_ASSERT_FALSE(parseProfile("nope", p));
}

void test_profile_bio_cut() {
  std::string body = "{\"data\":{\"username\":\"x\",\"bio\":\"" + std::string(3000, 'a') + "\"}}";
  Profile p;
  TEST_ASSERT_TRUE(parseProfile(body, p));
  TEST_ASSERT_EQUAL(int(kProfileBioBytes), int(p.bio.size()));
}

// A 640-emoji bio (the server's limit) loads whole and can be edited whole.
void test_profile_bio_keeps_640_emoji() {
  std::string bio;
  for (int i = 0; i < 640; ++i) bio += "\xF0\x9F\x98\x80";
  Profile p;
  TEST_ASSERT_TRUE(parseProfile("{\"data\":{\"username\":\"x\",\"bio\":\"" + bio + "\"}}", p));
  TEST_ASSERT_EQUAL_STRING(bio.c_str(), p.bio.c_str());
  InputLine in;
  in.setMaxBytes(kProfileBioBytes);
  in.setText(utf8ToU32(bio));
  TEST_ASSERT_EQUAL_STRING(bio.c_str(), in.utf8().c_str());
  in.insert(U'a');
  TEST_ASSERT_EQUAL(640, int(in.text().size()));
}

void test_blocked_users() {
  std::vector<std::string> b{"stale"};
  TEST_ASSERT_TRUE(parseBlockedUsers(fx::kOwnProfile, b));
  TEST_ASSERT_EQUAL(2, int(b.size()));
  TEST_ASSERT_EQUAL_STRING("troll", b[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Spammer", b[1].c_str());
  TEST_ASSERT_TRUE(hasUsername(b, "spammer"));
  TEST_ASSERT_TRUE(hasUsername(b, "TROLL"));
  TEST_ASSERT_FALSE(hasUsername(b, "troll2"));
  TEST_ASSERT_TRUE(parseBlockedUsers(fx::kProfileBare, b));
  TEST_ASSERT_EQUAL(0, int(b.size()));
  TEST_ASSERT_FALSE(parseBlockedUsers("nope", b));
}

void test_follow_page() {
  std::vector<Follow> v;
  std::string cursor;
  TEST_ASSERT_TRUE(parseFollowPage(fx::kFollowPage, v, cursor));
  TEST_ASSERT_EQUAL(2, int(v.size()));
  TEST_ASSERT_EQUAL_STRING("f1", v[0].followId.c_str());
  TEST_ASSERT_EQUAL_STRING("me1", v[0].followerId.c_str());
  TEST_ASSERT_EQUAL_STRING("u42", v[0].followedId.c_str());
  TEST_ASSERT_EQUAL_STRING("f2", cursor.c_str());
  TEST_ASSERT_TRUE(parseFollowPage(fx::kFollowPageEnd, v, cursor));
  TEST_ASSERT_EQUAL(1, int(v.size()));
  TEST_ASSERT_EQUAL_STRING("", cursor.c_str());
  TEST_ASSERT_FALSE(parseFollowPage("{\"data\":{}}", v, cursor));
}

void test_follow_id() {
  std::string id;
  TEST_ASSERT_TRUE(parseFollowId(fx::kFollowCreated, id));
  TEST_ASSERT_EQUAL_STRING("f9", id.c_str());
  TEST_ASSERT_FALSE(parseFollowId("{\"data\":{}}", id));
}

void test_follow_index() {
  FollowIndex ix;
  TEST_ASSERT_FALSE(ix.loaded());
  TEST_ASSERT_FALSE(ix.complete());
  std::vector<Follow> v;
  std::string cursor;
  parseFollowPage(fx::kFollowPage, v, cursor);
  ix.addPage(v, cursor);
  TEST_ASSERT_TRUE(ix.loaded());
  TEST_ASSERT_FALSE(ix.complete());
  TEST_ASSERT_EQUAL_STRING("f1", ix.followIdFor("u42").c_str());
  TEST_ASSERT_EQUAL_STRING("", ix.followIdFor("u44").c_str());
  parseFollowPage(fx::kFollowPageEnd, v, cursor);
  ix.addPage(v, cursor);
  TEST_ASSERT_TRUE(ix.complete());
  TEST_ASSERT_EQUAL_STRING("f3", ix.followIdFor("u44").c_str());
  ix.set("u50", "f50");
  TEST_ASSERT_EQUAL_STRING("f50", ix.followIdFor("u50").c_str());
  ix.erase("u42");
  TEST_ASSERT_EQUAL_STRING("", ix.followIdFor("u42").c_str());
  ix.clear();
  TEST_ASSERT_FALSE(ix.loaded());
  TEST_ASSERT_EQUAL_STRING("", ix.followIdFor("u44").c_str());
}

void test_follow_index_cap() {
  FollowIndex ix;
  std::vector<Follow> page;
  for (size_t i = 0; i < FollowIndex::kMax + 10; ++i)
    page.push_back(Follow{"f" + std::to_string(i), "me", "u" + std::to_string(i)});
  ix.addPage(page, "more");
  TEST_ASSERT_EQUAL_STRING("f0", ix.followIdFor("u0").c_str());
  TEST_ASSERT_EQUAL_STRING("", ix.followIdFor("u" + std::to_string(FollowIndex::kMax)).c_str());
  // Full: no further pages are wanted, but the list isn't known to be complete.
  TEST_ASSERT_TRUE(ix.loaded());
  TEST_ASSERT_FALSE(ix.complete());
  TEST_ASSERT_EQUAL_STRING("", ix.cursor().c_str());
}

void test_profile_text() {
  Profile p;
  parseProfile(fx::kProfileFull, p);
  const int64_t now = parseIsoMs("2026-10-31T12:00:00.000Z");
  std::u32string t = profileText(p, false, now);
  TEST_ASSERT_TRUE(hasLine(t, "Ann Example"));
  TEST_ASSERT_TRUE(hasLine(t, "@ann"));
  TEST_ASSERT_TRUE(hasLine(t, "\xF0\x9F\xA6\x89 Night Owls"));
  TEST_ASSERT_TRUE(hasLine(t, "#117 \xC2\xB7 joined Mar 2026"));
  TEST_ASSERT_TRUE(hasLine(t, "active 3h ago"));
  TEST_ASSERT_TRUE(hasLine(t, "40 followers \xC2\xB7 12 following \xC2\xB7 5 public entries"));
  TEST_ASSERT_TRUE(hasLine(t, "London, UK"));
  TEST_ASSERT_TRUE(hasLine(t, "my site https://ann.example"));
  TEST_ASSERT_TRUE(hasLine(t, "supporter"));
  TEST_ASSERT_TRUE(hasLine(t, "I like night walks."));
  TEST_ASSERT_TRUE(t.find(U"you blocked them") == std::u32string::npos);
  TEST_ASSERT_TRUE(t.find(U"you follow them") == std::u32string::npos);
  TEST_ASSERT_TRUE(hasLine(profileText(p, true, now), "you blocked them"));
  TEST_ASSERT_TRUE(hasLine(profileText(p, false, now, true), "you follow them"));
}

void test_profile_text_bare() {
  Profile p;
  parseProfile(fx::kProfileBare, p);
  std::u32string t = profileText(p, false, 0);
  TEST_ASSERT_TRUE(hasLine(t, "@bob"));
  TEST_ASSERT_TRUE(hasLine(t, "no bio"));
  TEST_ASSERT_TRUE(t.find(U"followers") == std::u32string::npos);
  TEST_ASSERT_TRUE(t.find(U"joined") == std::u32string::npos);
  TEST_ASSERT_TRUE(t.find(U"active") == std::u32string::npos);
  TEST_ASSERT_TRUE(t.find(U"supporter") == std::u32string::npos);
  p.followers = 1;
  p.lastActiveMs = 1000;
  t = profileText(p, false, 30000);
  TEST_ASSERT_TRUE(hasLine(t, "1 follower"));
  TEST_ASSERT_TRUE(hasLine(t, "active now"));
}

void test_profile_line_roles() {
  Profile p;
  parseProfile(fx::kProfileFull, p);
  auto lines = profileLines(p, true, 0, true);
  auto roleOf = [&](const char32_t* text) {
    for (const auto& l : lines)
      if (l.text == text) return int(l.role);
    return -1;
  };
  TEST_ASSERT_EQUAL(int(ProfileRole::Name), roleOf(U"Ann Example"));
  TEST_ASSERT_EQUAL(int(ProfileRole::Handle), roleOf(U"@ann"));
  TEST_ASSERT_EQUAL(int(ProfileRole::Following), roleOf(U"you follow them"));
  TEST_ASSERT_EQUAL(int(ProfileRole::Blocked), roleOf(U"you blocked them"));
  TEST_ASSERT_EQUAL(int(ProfileRole::Website), roleOf(U"my site https://ann.example"));
  // The lines joined are profileText.
  std::u32string joined;
  for (const auto& l : lines) joined += (joined.empty() ? U"" : U"\n") + l.text;
  TEST_ASSERT_TRUE(joined == profileText(p, true, 0, true));
}

void test_user_replies_parse() {
  std::vector<UserReply> r;
  std::string cur;
  TEST_ASSERT_TRUE(parseUserReplies(fx::kUserReplies, r, cur));
  TEST_ASSERT_EQUAL(2, int(r.size()));
  TEST_ASSERT_EQUAL_STRING("r1", r[0].replyId.c_str());
  TEST_ASSERT_EQUAL_STRING("p1", r[0].postId.c_str());
  TEST_ASSERT_EQUAL_STRING("First **reply**\nsecond line", r[0].content.c_str());
  TEST_ASSERT_TRUE(r[0].createdMs == parseIsoMs("2026-10-30T12:00:00.000Z"));
  TEST_ASSERT_EQUAL_STRING("r2", r[1].replyId.c_str());
  TEST_ASSERT_EQUAL_STRING("", r[1].content.c_str());
  TEST_ASSERT_EQUAL_STRING("r2", cur.c_str());
}

void test_user_replies_end_cursor() {
  std::vector<UserReply> r;
  std::string cur = "x";
  TEST_ASSERT_TRUE(parseUserReplies(fx::kUserRepliesEnd, r, cur));
  TEST_ASSERT_EQUAL(0, int(r.size()));
  TEST_ASSERT_EQUAL_STRING("", cur.c_str());
}

void test_user_replies_cut() {
  std::string big(2000, 'a');
  std::string body = std::string(R"({"data":[{"replyId":"r","postId":"p","content":")") + big +
                     R"(","deleted":false,"createdAt":"2026-10-30T12:00:00.000Z"}],"cursor":null})";
  std::vector<UserReply> r;
  std::string cur;
  TEST_ASSERT_TRUE(parseUserReplies(body, r, cur));
  TEST_ASSERT_TRUE(r[0].content.size() <= kUserReplyBytes);
}

void test_user_guilds_parse() {
  std::vector<UserGuild> g;
  TEST_ASSERT_TRUE(parseUserGuilds(fx::kUserGuilds, g));
  TEST_ASSERT_EQUAL(2, int(g.size()));
  TEST_ASSERT_EQUAL_STRING("night-owls", g[0].slug.c_str());
  TEST_ASSERT_EQUAL_STRING("Night Owls", g[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("🦉", g[0].icon.c_str());
  TEST_ASSERT_EQUAL_STRING("member", g[0].role.c_str());
  TEST_ASSERT_EQUAL_STRING("apprentice", g[1].role.c_str());
}

void test_user_guilds_missing_icon() {
  std::vector<UserGuild> g;
  TEST_ASSERT_TRUE(parseUserGuilds(fx::kUserGuildsNoIcon, g));
  TEST_ASSERT_EQUAL(1, int(g.size()));
  TEST_ASSERT_EQUAL_STRING("", g[0].icon.c_str());
  TEST_ASSERT_EQUAL_STRING("founder", g[0].role.c_str());
}

namespace {
ProfileFields baseFields() { return ProfileFields{"Ann", "old bio", "https://ann.example", "site", "London"}; }
}  // namespace

void test_profile_fields_from_profile() {
  Profile p;
  TEST_ASSERT_TRUE(parseProfile(fx::kProfileFull, p));
  ProfileFields f = profileFields(p);
  TEST_ASSERT_EQUAL_STRING("Ann Example", f.displayName.c_str());
  TEST_ASSERT_EQUAL_STRING("https://ann.example", f.websiteUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("my site", f.websiteName.c_str());
  TEST_ASSERT_EQUAL_STRING("London, UK", f.locationName.c_str());
}

void test_patch_only_changed() {
  ProfileFields a = baseFields(), b = a;
  b.bio = "new";
  TEST_ASSERT_EQUAL_STRING("{\"bio\":\"new\"}", profilePatchJson(a, b).c_str());
}

void test_patch_blank_is_null() {
  ProfileFields a = baseFields(), b = a;
  b.displayName = "  ";
  TEST_ASSERT_EQUAL_STRING("{\"displayName\":null}", profilePatchJson(a, b).c_str());
}

void test_patch_no_change_empty() {
  TEST_ASSERT_EQUAL_STRING("", profilePatchJson(baseFields(), baseFields()).c_str());
}

void test_patch_escapes() {
  ProfileFields a = baseFields(), b = a;
  b.bio = "say \"hi\"\nback\\slash";
  std::string j = profilePatchJson(a, b);
  JsonDocument d;
  TEST_ASSERT_FALSE(deserializeJson(d, j));
  TEST_ASSERT_EQUAL_STRING(b.bio.c_str(), d["bio"].as<const char*>());
}

void test_profile_field_error_limits() {
  ProfileFields f = baseFields();
  TEST_ASSERT_NULL(profileFieldError(f));
  f.displayName = std::string(64, 'a');
  TEST_ASSERT_NULL(profileFieldError(f));
  f.displayName += "a";
  TEST_ASSERT_EQUAL_STRING("Display name: 64 characters at most", profileFieldError(f));
  f = baseFields();
  f.bio = std::string(640, 'b');
  TEST_ASSERT_NULL(profileFieldError(f));
  f.bio += "b";
  TEST_ASSERT_EQUAL_STRING("Bio: 640 characters at most", profileFieldError(f));
  f = baseFields();
  f.websiteName = std::string(65, 'c');
  TEST_ASSERT_EQUAL_STRING("Website name: 64 characters at most", profileFieldError(f));
  f = baseFields();
  f.locationName = std::string(65, 'd');
  TEST_ASSERT_EQUAL_STRING("Location: 64 characters at most", profileFieldError(f));
}

void test_profile_field_error_url() {
  ProfileFields f = baseFields();
  f.websiteUrl = "ftp://x";
  TEST_ASSERT_EQUAL_STRING("Website must start with http:// or https://", profileFieldError(f));
  f.websiteUrl = "";
  TEST_ASSERT_NULL(profileFieldError(f));
  f.websiteUrl = "https://a";
  TEST_ASSERT_NULL(profileFieldError(f));
  f.websiteUrl = "http://a";
  TEST_ASSERT_NULL(profileFieldError(f));
  f.websiteUrl = "https://" + std::string(2041, 'a');
  TEST_ASSERT_EQUAL_STRING("Website: 2048 characters at most", profileFieldError(f));
}

void test_profile_field_error_counts_chars() {
  ProfileFields f = baseFields();
  f.bio.clear();
  for (int i = 0; i < 640; ++i) f.bio += "🙂";
  TEST_ASSERT_NULL(profileFieldError(f));
  f.bio += "🙂";
  TEST_ASSERT_EQUAL_STRING("Bio: 640 characters at most", profileFieldError(f));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_profile_full);
  RUN_TEST(test_profile_missing_fields);
  RUN_TEST(test_profile_bio_cut);
  RUN_TEST(test_blocked_users);
  RUN_TEST(test_follow_page);
  RUN_TEST(test_follow_id);
  RUN_TEST(test_follow_index);
  RUN_TEST(test_follow_index_cap);
  RUN_TEST(test_profile_text);
  RUN_TEST(test_profile_text_bare);
  RUN_TEST(test_user_replies_parse);
  RUN_TEST(test_user_replies_end_cursor);
  RUN_TEST(test_user_replies_cut);
  RUN_TEST(test_user_guilds_parse);
  RUN_TEST(test_user_guilds_missing_icon);
  RUN_TEST(test_profile_fields_from_profile);
  RUN_TEST(test_patch_only_changed);
  RUN_TEST(test_patch_blank_is_null);
  RUN_TEST(test_patch_no_change_empty);
  RUN_TEST(test_patch_escapes);
  RUN_TEST(test_profile_field_error_limits);
  RUN_TEST(test_profile_field_error_url);
  RUN_TEST(test_profile_field_error_counts_chars);
  RUN_TEST(test_profile_line_roles);
  RUN_TEST(test_profile_bio_keeps_640_emoji);
  return UNITY_END();
}
