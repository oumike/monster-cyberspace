#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rc {

struct Entry {
  std::string id, author, title, slug, contentWarning, body, guildSlug;
  std::vector<std::string> topics, attachments;  // attachments as display text
  int64_t createdMs = -1;
  int replies = 0;
  bool isPublic = false, nsfw = false, deleted = false, edited = false, truncated = false;
};

struct Reply {
  std::string id, postId, parentId, parentAuthor, author, body;
  std::vector<std::string> attachments;
  int64_t createdMs = -1;
  bool deleted = false, edited = false, truncated = false;
};

struct Bookmark {
  std::string id, postId, replyId;  // id = bookmarkId
};

}  // namespace rc
