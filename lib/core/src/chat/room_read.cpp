#include "chat/room_read.h"

namespace rc {

bool roomUnread(const RoomInfo& r, const std::map<std::string, int64_t>& readAt) {
  auto it = readAt.find(r.slug);
  return it == readAt.end() ? r.lastMessageAt > 0 : r.lastMessageAt > it->second;
}

std::vector<std::string> unreadRooms(const std::vector<RoomInfo>& rooms,
                                     const std::map<std::string, int64_t>& readAt) {
  std::vector<std::string> out;
  for (const auto& r : rooms)
    if (roomUnread(r, readAt)) out.push_back(r.slug);
  return out;
}

}  // namespace rc
