#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "people/profile.h"

namespace rc {

// The Profile screen's text:
//   "Display Name" (only when set), "@user",
//   "<icon> <guild name>" (when the badge is set),
//   "#<serial> · joined Mar 2026" (either part when known),
//   "active 3h ago" / "active now" (when lastActiveMs is known),
//   "12 followers · 40 following · 5 public entries" (the known counts),
//   "<location>", "<websiteName> <websiteUrl>",
//   "supporter", "you follow them" (when following), "you blocked them"
//   (when blocked),
//   "", then the bio as markdownToText (or "no bio").
std::u32string profileText(const Profile& p, bool blocked, int64_t nowMs,
                           bool following = false);

// What a line of the profile is, so the screen can colour it.
enum class ProfileRole : uint8_t {
  Name, Handle, Guild, Meta, Active, Counts, Website, Supporter, Following, Blocked, Bio
};
struct ProfileLine {
  std::u32string text;  // the bio may hold several paragraphs
  ProfileRole role;
};
// The same text as profileText, one entry per line (joined with "\n").
std::vector<ProfileLine> profileLines(const Profile& p, bool blocked, int64_t nowMs,
                                      bool following = false);

}  // namespace rc
