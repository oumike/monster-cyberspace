#pragma once

#include <cstdint>
#include <string>

#include "guild/guild.h"
#include "ui/feed_layout.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"

namespace rc {

// line1 "🦉 Night Owls · 42+7" (+ " · member" etc. when role set; "+N" only when N>0);
// no icon → no leading icon and space; no name → slug. line2 dim: the bio's first
// non-empty markdownToText line, else "no bio". Cut like layoutFeedRow.
FeedRow layoutGuildRow(const Guild& g, int cols, const Theme& t, int headCols = -1);

// "@user", right: "role · 3mo" (relativeAge of joinedMs).
ListItem memberItem(const Member& m, int64_t nowMs);

// The Info page text: "<icon> <name>", "founded by @x", "42 members · 7 apprentices",
// "founded Mar 2026" (when createdMs ≥ 0), "you: member" (only when role set), "", the bio
// as markdownToText, then "", "linkText <url>" when link is set.
std::u32string guildInfoText(const Guild& g);

// "@user in Night Owls: <preview>"; no guildName → "@user in a guild: <preview>".
std::string guildNoticeText(const std::string& actor, const std::string& guildName,
                            const std::string& preview);

}  // namespace rc
