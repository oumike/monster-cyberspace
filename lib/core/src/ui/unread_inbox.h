#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rc {

// One row of the Unread screen: an unread C-Mail conversation, cIRC mention
// or Feed/guild notice.
enum class UnreadKind : uint8_t { Cmail, Mention, Feed };

struct UnreadItem {
  UnreadKind kind = UnreadKind::Cmail;
  std::string target;  // conversation id | room slug | "postId\nnoticeId"
  std::string text;    // the display line
  int64_t at = 0;      // epoch ms; 0 = unknown
  int count = 1;       // C-Mail: the conversation's unread messages
};

// The three sources, each newest first, merged newest first. Ties keep the
// order cmail, mentions, feed; unknown times (0) follow the dated items in
// that same source order.
std::vector<UnreadItem> mergeUnread(std::vector<UnreadItem> cmail, std::vector<UnreadItem> mentions,
                                    std::vector<UnreadItem> feed);

// The sum of the items' counts.
int unreadTotal(const std::vector<UnreadItem>& items);

}  // namespace rc
