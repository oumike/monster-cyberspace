#include "ui/unread_inbox.h"

#include <algorithm>
#include <iterator>

namespace rc {

std::vector<UnreadItem> mergeUnread(std::vector<UnreadItem> cmail, std::vector<UnreadItem> mentions,
                                    std::vector<UnreadItem> feed) {
  std::vector<UnreadItem> out = std::move(cmail);
  out.reserve(out.size() + mentions.size() + feed.size());
  std::move(mentions.begin(), mentions.end(), std::back_inserter(out));
  std::move(feed.begin(), feed.end(), std::back_inserter(out));
  auto dated = std::stable_partition(out.begin(), out.end(), [](const UnreadItem& i) { return i.at > 0; });
  std::stable_sort(out.begin(), dated, [](const UnreadItem& a, const UnreadItem& b) { return a.at > b.at; });
  return out;
}

int unreadTotal(const std::vector<UnreadItem>& items) {
  int n = 0;
  for (const auto& i : items) n += i.count;
  return n;
}

int unreadCmailCount(const std::vector<ConversationInfo>& conversations) {
  int n = 0;
  for (const auto& c : conversations)
    if (c.unread > 0 && !c.partnerDeleted) n += c.unread;
  return n;
}

int unreadCount(const std::vector<ConversationInfo>& conversations, size_t mentions, size_t notices) {
  return unreadCmailCount(conversations) + int(mentions) + int(notices);
}

}  // namespace rc
