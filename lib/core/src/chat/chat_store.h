#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "chat/message.h"

namespace rc {

// Messages kept ascending by (timestamp, id), unique by id, capped at `cap`
// (oldest dropped). Pointers and references returned here are invalidated by
// the next mutation.
class ChatStore {
 public:
  explicit ChatStore(size_t cap = 1000);

  void clear();
  void replaceAll(std::vector<CircMessage> msgs);
  CircMessage& upsert(CircMessage msg);
  CircMessage* find(const std::string& id);
  bool remove(const std::string& id);

  const std::deque<CircMessage>& messages() const { return msgs_; }
  size_t size() const { return msgs_.size(); }
  // -1 when empty.
  int64_t newestTimestamp() const;
  int64_t oldestTimestamp() const;
  uint32_t version() const { return version_; }

 private:
  void trim();

  size_t cap_;
  std::deque<CircMessage> msgs_;
  uint32_t version_ = 0;
};

}  // namespace rc
