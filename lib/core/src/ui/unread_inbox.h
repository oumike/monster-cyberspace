#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cs/api_parse.h"

namespace rc {

// One row of the Unread screen: an unread C-Mail conversation, cIRC mention,
// Feed/guild notice, or a follower or poke (People, which opens the profile).
enum class UnreadKind : uint8_t { Cmail, Mention, Feed, People };

struct UnreadItem {
  UnreadKind kind = UnreadKind::Cmail;
  std::string target;  // conversation id | room slug | "postId\nnoticeId" | "user\nnoticeId"
  std::string text;    // the display line
  int64_t at = 0;      // epoch ms; 0 = unknown
  int count = 1;       // C-Mail: the conversation's unread messages
};

// The three sources, each newest first, merged newest first (People rows
// come in with feed). Ties keep the order cmail, mentions, feed; unknown times (0) follow the dated items in
// that same source order.
std::vector<UnreadItem> mergeUnread(std::vector<UnreadItem> cmail, std::vector<UnreadItem> mentions,
                                    std::vector<UnreadItem> feed);

// The sum of the items' counts.
int unreadTotal(const std::vector<UnreadItem>& items);

// The unread C-Mail messages the Unread screen lists: conversations with
// unread messages whose partner isn't deleted.
int unreadCmailCount(const std::vector<ConversationInfo>& conversations);
// unreadTotal of the rows the screen would build, without building them (Home
// shows it every frame): the C-Mail count plus one per mention and notice.
int unreadCount(const std::vector<ConversationInfo>& conversations, size_t mentions, size_t notices);

}  // namespace rc
