#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

struct EntryDraft {
  std::string title, body, topics, cw;  // topics: comma-separated as typed
  bool isPublic = false, nsfw = false;
};

constexpr size_t kBodyMaxChars = 4096;

// Trims; strips a leading '#'; lower-cases ASCII; drops empties.
std::vector<std::string> splitTopics(std::string_view s);
// "" when valid, else the message to show. Lengths count code points; the
// title and cw are counted trimmed.
std::string validateDraft(const EntryDraft& d);
// Trims the title and cw. Leaves out an empty (or whitespace-only) title,
// empty topics and an empty cw; leaves out
// isPublic and isNSFW when false.
std::string entryJson(const EntryDraft& d);
// parentReplyId only when non-empty.
std::string replyJson(const std::string& postId, const std::string& body,
                      const std::string& parentReplyId);
// {} when reason is empty.
std::string flagJson(const std::string& reason);
// {postId, type:"post"} when postId is set, else {replyId, type:"reply"}.
std::string bookmarkJson(const std::string& postId, const std::string& replyId);

}  // namespace rc
