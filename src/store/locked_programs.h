// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "programs/programs.h"

namespace rc {

// Everything the Programs screens read and the network side writes.
struct ProgramsState {
  std::vector<ProgramInfo> list;  // newest first
  std::string cursor;             // the next page's `before`; "" at the end
  bool loaded = false;            // the first page came in
  ProgramSource open;             // the program on screen
  int openRelease = 0;            // the release asked for (0: the current one)
  bool loading = false;
};

class LockedPrograms {
 public:
  // Runs f(ProgramsState&) under the lock. Keep f short: no I/O, and never
  // feedSource().command() inside it.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  ProgramsState state_;
};

LockedPrograms& lockedPrograms();

}  // namespace rc
