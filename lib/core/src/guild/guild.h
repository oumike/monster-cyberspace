#pragma once

#include <cstdint>
#include <string>

namespace rc {

struct Guild {
  std::string id, slug, name, icon, founder, bio, link, linkText;
  std::string role;  // "founder" | "member" | "apprentice" | "" (not in it / unknown)
  int64_t createdMs = -1;
  int members = 0, apprentices = 0;
};

struct MyGuild {
  std::string slug, name, icon, role;
};

struct Member {
  std::string id, username, role;  // id = membershipId
  int64_t joinedMs = -1;
};

}  // namespace rc
