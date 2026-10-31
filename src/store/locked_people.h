#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "people/follow_index.h"
#include "people/profile.h"

namespace rc {

// Everything the Profile screen reads and the network side writes.
struct PeopleState {
  Profile profile;              // the open profile
  std::string wanted;           // the username asked for (set before the load)
  bool loaded = false;          // profile holds `wanted`
  bool loading = false;
  FollowIndex follows;          // who I follow
  std::vector<std::string> blocked;  // usernames I blocked on the website
};

class LockedPeople {
 public:
  // Runs f(PeopleState&) under the lock. Keep f short: no I/O, and never
  // feedSource().command() inside it.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  PeopleState state_;
};

LockedPeople& lockedPeople();

}  // namespace rc
