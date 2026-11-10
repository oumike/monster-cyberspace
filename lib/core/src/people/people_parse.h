#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "people/profile.h"

namespace rc {

// Bios are cut to this many bytes (the API allows 640 characters).
constexpr size_t kProfileBioBytes = 2560;  // 640 characters of up to 4 bytes

// A user's replies are cut to this many bytes for their list rows.
constexpr size_t kUserReplyBytes = 512;

// GET /v1/users/:username (and /me): {"data":{...}}. False without a username.
bool parseProfile(std::string_view body, Profile& out);
// GET /v1/users/me: data.blockedUsers; empty when absent.
bool parseBlockedUsers(std::string_view body, std::vector<std::string>& out);
// GET /v1/follows. `cursor` is "" when null or absent.
bool parseFollowPage(std::string_view body, std::vector<Follow>& out, std::string& cursor);
// Usernames match without regard to ASCII case.
bool hasUsername(const std::vector<std::string>& names, const std::string& username);
// POST /v1/follows: data.followId.
bool parseFollowId(std::string_view body, std::string& followId);

// GET /v1/users/:username/replies; deleted replies skipped. `cursor` "" when null.
bool parseUserReplies(std::string_view body, std::vector<UserReply>& out, std::string& cursor);
// GET /v1/users/:username/guilds, in the API's order.
bool parseUserGuilds(std::string_view body, std::vector<UserGuild>& out);

}  // namespace rc
