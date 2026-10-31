#pragma once

#include <cstdint>
#include <string>

#include "people/profile.h"

namespace rc {

// The Profile screen's text:
//   "Display Name" (only when set), "@user",
//   "<icon> <guild name>" (when the badge is set),
//   "#<serial> · joined Mar 2026" (either part when known),
//   "active 3h ago" / "active now" (when lastActiveMs is known),
//   "12 followers · 40 following · 5 public entries" (the known counts),
//   "<location>", "<websiteName> <websiteUrl>",
//   "supporter", "you blocked them" (when blocked),
//   "", then the bio as markdownToText (or "no bio").
std::u32string profileText(const Profile& p, bool blocked, int64_t nowMs);

}  // namespace rc
