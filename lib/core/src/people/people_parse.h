#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "people/profile.h"

namespace rc {

// Bios are cut to this many bytes (the API allows 640 characters).
constexpr size_t kProfileBioBytes = 1024;

// GET /v1/users/:username (and /me): {"data":{...}}. False without a username.
bool parseProfile(std::string_view body, Profile& out);
// GET /v1/users/me: data.blockedUsers; empty when absent.
bool parseBlockedUsers(std::string_view body, std::vector<std::string>& out);
// GET /v1/follows. `cursor` is "" when null or absent.
bool parseFollowPage(std::string_view body, std::vector<Follow>& out, std::string& cursor);
// POST /v1/follows: data.followId.
bool parseFollowId(std::string_view body, std::string& followId);

}  // namespace rc
