#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "people/profile.h"

namespace rc {

// Who I follow, by user id, with the follow id an unfollow needs. Profiles
// don't say whether I follow someone, so this is loaded from my following
// list (GET /v1/follows?type=following) a page at a time.
class FollowIndex {
 public:
  static constexpr size_t kMax = 500;

  // A page of my following list; `cursor` "" marks the end.
  void addPage(const std::vector<Follow>& page, const std::string& cursor);
  // After a follow or unfollow of mine.
  void set(const std::string& userId, const std::string& followId);
  void erase(const std::string& userId);
  void clear();

  // "" when not followed (or not loaded that far).
  std::string followIdFor(const std::string& userId) const;
  bool loaded() const { return loaded_; }
  // Every page is in: a missing id means "not following".
  bool complete() const { return loaded_ && cursor_.empty() && !full_; }
  // The next page to fetch; "" when there's nothing more to ask for.
  const std::string& cursor() const { return cursor_; }

 private:
  std::map<std::string, std::string> ids_;  // followedId → followId
  std::string cursor_;
  bool loaded_ = false;
  bool full_ = false;  // hit kMax: stopped paging early
};

}  // namespace rc
