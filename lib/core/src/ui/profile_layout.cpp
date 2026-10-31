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

std::u32string profileText(const Profile& p, bool blocked, int64_t nowMs) {
  std::u32string t;
  if (!p.displayName.empty()) t += utf8ToU32(p.displayName) + U"\n";
  t += U"@" + utf8ToU32(p.username) + U"\n";
  if (!p.guildName.empty() || !p.guildSlug.empty()) {
    if (!p.guildIcon.empty()) t += utf8ToU32(p.guildIcon) + U" ";
    t += utf8ToU32(p.guildName.empty() ? p.guildSlug : p.guildName) + U"\n";
  }

  std::u32string line;
  if (p.serial >= 0) line = U"#" + num(p.serial);
  if (p.createdMs >= 0) join(line, U"joined " + monthYear(p.createdMs));
  if (!line.empty()) t += line + U"\n";

  if (p.lastActiveMs >= 0) {
    std::string age = relativeAge(p.lastActiveMs, nowMs);
    // relativeAge gives "now", "5m"/"3h"/"2d", or a date such as "14 Oct".
    bool date = age.find(' ') != std::string::npos;
    t += U"active " + utf8ToU32(age) + (age == "now" || date ? U"\n" : U" ago\n");
  }

  line.clear();
  if (p.followers >= 0) join(line, num(p.followers) + (p.followers == 1 ? U" follower" : U" followers"));
  if (p.following >= 0) join(line, num(p.following) + U" following");
  if (p.publicPosts >= 0)
    join(line, num(p.publicPosts) + (p.publicPosts == 1 ? U" public entry" : U" public entries"));
  if (!line.empty()) t += line + U"\n";

  if (!p.locationName.empty()) t += utf8ToU32(p.locationName) + U"\n";
  if (!p.websiteUrl.empty()) {
    if (!p.websiteName.empty()) t += utf8ToU32(p.websiteName) + U" ";
    t += utf8ToU32(p.websiteUrl) + U"\n";
  }
  if (p.supporter) t += U"supporter\n";
  if (blocked) t += U"you blocked them\n";

  std::u32string bio = markdownToText(p.bio);
  t += U"\n" + (bio.empty() ? std::u32string(U"no bio") : bio);
  return t;
}

}  // namespace rc
