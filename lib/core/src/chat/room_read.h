#pragma once

#include <cstdint>
#include <map>
#include <string>
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

}  // namespace rc
