#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rc {

struct CircMessage {
  std::string id, userId, username, content;
  int64_t timestamp = 0;
  bool isChatAdmin = false, isAction = false, isDice = false,
       isEightball = false, isFortune = false, deleted = false;
  std::vector<std::string> styles;
  std::string imageUrl, gifUrl, eightballAnswer, fortuneText;
  struct Audio {
    std::string src, origin, artist, title, genre;
  } audio;
  bool hasAudio = false;
};

}  // namespace rc
