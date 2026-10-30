#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "guild/guild.h"

namespace rc {

// For a list, `cursor` is "" when null or absent.
bool parseGuildPage(std::string_view body, std::vector<Guild>& out, std::string& cursor);
// Get Guild: the guild object plus isMember/role.
bool parseGuild(std::string_view body, Guild& out);
bool parseMyGuilds(std::string_view body, std::vector<MyGuild>& out);
bool parseMembers(std::string_view body, std::vector<Member>& out, std::string& cursor);
// join/promote: {"data":{"guildId":..,"role":..}}
bool parseGuildRole(std::string_view body, std::string& role);

}  // namespace rc
