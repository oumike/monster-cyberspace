#include "ui/guild_layout.h"

#include <algorithm>
#include <ctime>

#include "feed/markdown_text.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace rc {

namespace {

constexpr char32_t kSep[] = U" \u00b7 ";

std::u32string cut(std::u32string s, int cols) {
  s.resize(fitChars(s, std::max(cols, 0)));
  return s;
}

std::u32string bioText(const Guild& g) { return markdownToText(g.bio); }

}  // namespace

FeedRow layoutGuildRow(const Guild& g, int cols, const Theme& t, int headCols) {
  FeedRow r;
  if (!g.icon.empty()) r.line1 = utf8ToU32(g.icon) + U" ";
  r.line1 += utf8ToU32(g.name.empty() ? g.slug : g.name) + kSep +
             utf8ToU32(std::to_string(g.members));
  if (g.apprentices > 0) r.line1 += U"+" + utf8ToU32(std::to_string(g.apprentices));
  if (!g.role.empty()) r.line1 += kSep + utf8ToU32(g.role);

  r.line2 = U"no bio";
  std::u32string text = bioText(g);
  size_t start = 0;
  while (start <= text.size()) {
    size_t nl = text.find(U'\n', start);
    if (nl == std::u32string::npos) nl = text.size();
    std::u32string line = text.substr(start, nl - start);
    if (line.find_first_not_of(U" \t\r") != std::u32string::npos) {
      r.line2 = line;
      break;
    }
    start = nl + 1;
  }
  r.line1 = cut(r.line1, headCols < 0 ? cols : headCols);
  r.line2 = cut(r.line2, cols);
  r.fg1 = t.fg;
  r.fg2 = t.dim;
  return r;
}

ListItem memberItem(const Member& m, int64_t nowMs) {
  ListItem it;
  it.label = U"@" + utf8ToU32(m.username);
  std::u32string right = utf8ToU32(m.role);
  if (m.joinedMs >= 0) {
    if (!right.empty()) right += kSep;
    right += utf8ToU32(relativeAge(m.joinedMs, nowMs));
  }
  it.right = right;
  return it;
}

std::u32string guildInfoText(const Guild& g) {
  std::u32string t;
  if (!g.icon.empty()) t += utf8ToU32(g.icon) + U" ";
  t += utf8ToU32(g.name.empty() ? g.slug : g.name) + U"\n";
  if (!g.founder.empty()) t += U"founded by @" + utf8ToU32(g.founder) + U"\n";
  t += utf8ToU32(std::to_string(g.members)) + U" members" + kSep +
       utf8ToU32(std::to_string(g.apprentices)) + U" apprentices\n";
  if (g.createdMs >= 0) {
    static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::time_t s = static_cast<std::time_t>(g.createdMs / 1000);
    std::tm tm{};
    gmtime_r(&s, &tm);
    t += U"founded " + utf8ToU32(kMonths[tm.tm_mon % 12]) + U" " +
         utf8ToU32(std::to_string(tm.tm_year + 1900)) + U"\n";
  }
  if (!g.role.empty()) t += U"you: " + utf8ToU32(g.role) + U"\n";
  t += U"\n" + bioText(g);
  if (!g.link.empty()) {
    t += U"\n\n";
    if (!g.linkText.empty()) t += utf8ToU32(g.linkText) + U" ";
    t += utf8ToU32(g.link);
  }
  return t;
}

std::string guildNoticeText(const std::string& actor, const std::string& guildName,
                            const std::string& preview) {
  return "@" + actor + " in " + (guildName.empty() ? std::string("a guild") : guildName) + ": " +
         preview;
}

}  // namespace rc
