#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "feed/entry.h"

namespace rc {

// For a list, `cursor` is "" when null or absent.

// UTC "2026-03-27T10:12:01.516Z" → epoch ms; -1 if malformed.
int64_t parseIsoMs(std::string_view iso);
bool parseFeedPage(std::string_view body, std::vector<Entry>& out, std::string& cursor);
bool parseEntry(std::string_view body, Entry& out);
bool parseReplies(std::string_view body, std::vector<Reply>& out, std::string& cursor);
bool parseReply(std::string_view body, Reply& out);
// {"data":{postId|replyId|bookmarkId: ...}} → the first present of those keys.
bool parseCreatedId(std::string_view body, std::string& id);
bool parseBookmarks(std::string_view body, std::vector<Bookmark>& out, std::string& cursor);
bool parseWatches(std::string_view body, std::vector<std::string>& postIds, std::string& cursor);
bool parseWatching(std::string_view body, bool& watching);

}  // namespace rc
