#pragma once

#include <cstdint>
#include <string>

namespace rc {

// GET /v1/users/:username. Every field but username is optional; counts are
// -1 and times -1 when absent.
struct Profile {
  std::string userId, username, displayName, bio;
  std::string guildSlug, guildIcon, guildName;  // the badge: the guild they're a member of
  std::string locationName, websiteUrl, websiteName;
  int serial = -1;
  int followers = -1, following = -1, publicPosts = -1;
  int64_t createdMs = -1, lastActiveMs = -1;
  bool supporter = false;
};

// GET /v1/follows: one follow document. Usernames are not in it.
struct Follow {
  std::string followId, followerId, followedId;
};

}  // namespace rc
