#include "ui/profile_layout.h"

#include <ctime>

#include "feed/markdown_text.h"
#include "ui/feed_layout.h"
#include "util/utf8.h"

namespace rc {

namespace {

constexpr char32_t kSep[] = U" \u00b7 ";

std::u32string num(int n) { return utf8ToU32(std::to_string(n)); }

std::u32string monthYear(int64_t ms) {
  static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  std::time_t s = static_cast<std::time_t>(ms / 1000);
  std::tm tm{};
  gmtime_r(&s, &tm);
  return utf8ToU32(kMonths[tm.tm_mon % 12]) + U" " + num(tm.tm_year + 1900);
}

void join(std::u32string& line, const std::u32string& part) {
  if (!line.empty()) line += kSep;
  line += part;
}

}  // namespace

std::vector<ProfileLine> profileLines(const Profile& p, bool blocked, int64_t nowMs,
                                      bool following) {
  std::vector<ProfileLine> out;
  auto add = [&](std::u32string text, ProfileRole role) {
    out.push_back(ProfileLine{std::move(text), role});
  };
  if (!p.displayName.empty()) add(utf8ToU32(p.displayName), ProfileRole::Name);
  add(U"@" + utf8ToU32(p.username), ProfileRole::Handle);
  if (!p.guildName.empty() || !p.guildSlug.empty()) {
    std::u32string g;
    if (!p.guildIcon.empty()) g = utf8ToU32(p.guildIcon) + U" ";
    add(g + utf8ToU32(p.guildName.empty() ? p.guildSlug : p.guildName), ProfileRole::Guild);
  }

  std::u32string line;
  if (p.serial >= 0) line = U"#" + num(p.serial);
  if (p.createdMs >= 0) join(line, U"joined " + monthYear(p.createdMs));
  if (!line.empty()) add(line, ProfileRole::Meta);

  if (p.lastActiveMs >= 0) {
    std::string age = relativeAge(p.lastActiveMs, nowMs);
    // relativeAge gives "now", "5m"/"3h"/"2d", or a date such as "14 Oct".
    bool date = age.find(' ') != std::string::npos;
    add(U"active " + utf8ToU32(age) + (age == "now" || date ? U"" : U" ago"), ProfileRole::Active);
  }

  line.clear();
  if (p.followers >= 0) join(line, num(p.followers) + (p.followers == 1 ? U" follower" : U" followers"));
  if (p.following >= 0) join(line, num(p.following) + U" following");
  if (p.publicPosts >= 0)
    join(line, num(p.publicPosts) + (p.publicPosts == 1 ? U" public entry" : U" public entries"));
  if (!line.empty()) add(line, ProfileRole::Counts);

  if (!p.locationName.empty()) add(utf8ToU32(p.locationName), ProfileRole::Meta);
  if (!p.websiteUrl.empty()) {
    std::u32string w;
    if (!p.websiteName.empty()) w = utf8ToU32(p.websiteName) + U" ";
    add(w + utf8ToU32(p.websiteUrl), ProfileRole::Website);
  }
  if (p.supporter) add(U"supporter", ProfileRole::Supporter);
  if (following) add(U"you follow them", ProfileRole::Following);
  if (blocked) add(U"you blocked them", ProfileRole::Blocked);

  add(U"", ProfileRole::Bio);
  std::u32string bio = markdownToText(p.bio);
  if (bio.empty()) add(U"no bio", ProfileRole::Meta);
  else add(bio, ProfileRole::Bio);
  return out;
}

std::u32string profileText(const Profile& p, bool blocked, int64_t nowMs, bool following) {
  std::u32string t;
  for (const auto& l : profileLines(p, blocked, nowMs, following)) {
    t += l.text;
    t += U'\n';
  }
  if (!t.empty()) t.pop_back();
  return t;
}

}  // namespace rc
