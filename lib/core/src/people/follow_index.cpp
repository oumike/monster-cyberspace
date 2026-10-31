#include "people/follow_index.h"

namespace rc {

void FollowIndex::addPage(const std::vector<Follow>& page, const std::string& cursor) {
  loaded_ = true;
  cursor_ = cursor;
  for (const Follow& f : page) {
    if (ids_.size() >= kMax) {
      // Full: stop paging. Later lookups past the cap read as "not following",
      // so complete() stays false and the screen says it doesn't know.
      cursor_.clear();
      full_ = true;
      return;
    }
    ids_[f.followedId] = f.followId;
  }
}

void FollowIndex::set(const std::string& userId, const std::string& followId) {
  if (!userId.empty() && !followId.empty()) ids_[userId] = followId;
}

void FollowIndex::erase(const std::string& userId) { ids_.erase(userId); }

void FollowIndex::clear() {
  ids_.clear();
  cursor_.clear();
  loaded_ = false;
  full_ = false;
}

std::string FollowIndex::followIdFor(const std::string& userId) const {
  auto it = ids_.find(userId);
  return it == ids_.end() ? std::string() : it->second;
}

}  // namespace rc
