#pragma once

#include <cstdint>
#include <string>

#include "chat/chat_store.h"
#include "chat/presence.h"
#include "net/sse_parser.h"

namespace rc {

struct RtdbQuery {
  bool orderByTimestamp = false;
  int64_t startAfter = -1;
  int limitToLast = 0;
};

std::string rtdbStreamUrl(const std::string& base, const std::string& path,
                          const std::string& idToken, const RtdbQuery& q);

enum class StreamResult { Changed, Unchanged, KeepAlive, Cancel, AuthRevoked, BadData };

// Applies a Firebase RTDB stream event to the chat store. A `put` at `/`
// upserts every message in the window and never clears: the stream opens with
// startAfter=<newest stored>, so its window does not cover REST history.
// `put` at `/<id>` replaces that message (null removes it); `patch` merges.
// Malformed data returns BadData and leaves the store untouched.
StreamResult applyChatEvent(ChatStore& store, const SseEvent& ev);

// Same path rules, keyed by userId, except that a `put` at `/` replaces the
// whole map (null clears it).
StreamResult applyPresenceEvent(PresenceMap& map, const SseEvent& ev);

}  // namespace rc
