#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "notes/notes.h"

namespace rc {

// Everything the Notes screens read and the network side writes.
struct NotesState {
  std::vector<NoteRow> list;
  std::string listCursor;
  Note open;               // the note on screen (latest or an old revision)
  int latestRevision = 0;  // of `open`'s note
  std::vector<NoteRow> history;
  std::string historyCursor;
  std::string historyId;   // whose revisions `history` holds
  bool loading = false;
};

class LockedNotes {
 public:
  // Runs f(NotesState&) under the lock. Keep f short: no I/O, and never
  // feedSource().command() inside it.
  template <class F>
  auto with(F f) {
    std::lock_guard<std::mutex> g(mu_);
    return f(state_);
  }

 private:
  std::mutex mu_;
  NotesState state_;
};

LockedNotes& lockedNotes();

}  // namespace rc
