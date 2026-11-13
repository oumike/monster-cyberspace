// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "search/search.h"

namespace rc {

// Everything the Search screens read and the network side writes.
struct SearchState {
  std::string query;                 // the last query run
  std::vector<SearchHit> users, posts, replies;  // type=all's groups
  bool searched = false;             // a type=all answer came in
  // The open "More…" list: one type, paged.
  std::string listType;              // "users", "posts" or "replies"; "" none
  std::vector<SearchHit> list;
  std::string cursor;                // the next page; "" at the end
  bool listLoaded = false;
  bool loading = false;
};

class LockedSearch {
 public:
  // Runs f(SearchState&) under the lock. Keep f short: no I/O, and never
  // feedSource().command() inside it.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  SearchState state_;
};

LockedSearch& lockedSearch();

}  // namespace rc
