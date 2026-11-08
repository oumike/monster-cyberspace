#include "chat/room_read.h"

#include <cstdlib>

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

std::map<std::string, int64_t> roomReadMarks(const std::vector<RoomInfo>& rooms,
                                             const std::map<std::string, int64_t>& readAt) {
  std::map<std::string, int64_t> out;
  for (const auto& r : rooms) {
    auto it = readAt.find(r.slug);
    if (it != readAt.end()) out[r.slug] = it->second;
  }
  return out;
}

std::string encodeReadMarks(const std::string& user, const std::map<std::string, int64_t>& marks,
                            size_t maxBytes) {
  std::string out = user + "\n";
  for (const auto& kv : marks) {
    if (kv.first.empty() || kv.first.find_first_of(" \n") != std::string::npos) continue;
    std::string line = kv.first + " " + std::to_string(kv.second) + "\n";
    if (out.size() + line.size() > maxBytes) break;
    out += line;
  }
  return out;
}

bool decodeReadMarks(std::string_view text, std::string& user,
                     std::map<std::string, int64_t>& marks) {
  size_t nl = text.find('\n');
  if (nl == std::string_view::npos || nl == 0) return false;
  user.assign(text.substr(0, nl));
  marks.clear();
  for (size_t pos = nl + 1; pos < text.size();) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string line(text.substr(pos, end - pos));
    pos = end + 1;
    size_t sp = line.find(' ');
    if (sp == 0 || sp == std::string::npos || sp + 1 >= line.size()) continue;
    char* stop = nullptr;
    long long ms = std::strtoll(line.c_str() + sp + 1, &stop, 10);
    if (*stop != '\0') continue;
    marks[line.substr(0, sp)] = ms;
  }
  return true;
}

}  // namespace rc
