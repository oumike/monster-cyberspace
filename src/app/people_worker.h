#pragma once

#if !RC_FAKE_API

#include <string>

#include "app/messages.h"
#include "cs/cs_api.h"

namespace rc {

// The People side of the network task: profiles, follow, unfollow, poke and
// a user's entries. Writes come only from commands; nothing is retried.
// There is no block endpoint: the blocked list is read from /users/me.
class PeopleWorker {
 public:
  explicit PeopleWorker(CsApi& api);
  void handle(const Command& c);  // the People commands only
  void signOut();                 // lockedPeople cleared

  static bool isPeopleCommand(CmdType t);

 private:
  void openProfile(const std::string& username);
  // My following list, up to kFollowPages pages from where it left off.
  void loadFollowing();
  // Re-reads the open profile after a write (its counts change); failures
  // are ignored, the write itself succeeded.
  void refreshProfile(const std::string& username);
  void follow(const Command& c);
  void unfollow(const Command& c);
  void poke(const Command& c);
  void openUserPosts(const std::string& username, bool append);

  CsApi& api_;
};

}  // namespace rc

#endif
