#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "cs/api_parse.h"

namespace rc {

// A room is unread when its last message is newer than when it was last
// read (readAt: slug → lastMessageAt then); a room never read is unread once
// it has any message.
bool roomUnread(const RoomInfo& r, const std::map<std::string, int64_t>& readAt);

// The slugs of the unread rooms, in the order given.
std::vector<std::string> unreadRooms(const std::vector<RoomInfo>& rooms,
                                     const std::map<std::string, int64_t>& readAt);

// The read marks of the listed rooms only (readAt also holds C-Mail ids).
std::map<std::string, int64_t> roomReadMarks(const std::vector<RoomInfo>& rooms,
                                             const std::map<std::string, int64_t>& readAt);

// Read marks kept across reboots: the user they belong to on the first line,
// then one "slug ms" line per room, cut to at most maxBytes.
std::string encodeReadMarks(const std::string& user, const std::map<std::string, int64_t>& marks,
                            size_t maxBytes);
// False without a user line; malformed room lines are skipped.
bool decodeReadMarks(std::string_view text, std::string& user,
                     std::map<std::string, int64_t>& marks);

}  // namespace rc
