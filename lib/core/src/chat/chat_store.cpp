#include "chat/chat_store.h"

#include <algorithm>
#include <unordered_map>

namespace rc {

namespace {

bool before(const CircMessage& a, const CircMessage& b) {
  if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
  return a.id < b.id;
}

}  // namespace

ChatStore::ChatStore(size_t cap) : cap_(cap < 1 ? 1 : cap) {}

void ChatStore::clear() {
  msgs_.clear();
  ++version_;
}

void ChatStore::replaceAll(std::vector<CircMessage> msgs) {
  // Later entries win for duplicate ids.
  std::unordered_map<std::string, size_t> last;
  for (size_t i = 0; i < msgs.size(); ++i) last[msgs[i].id] = i;
  msgs_.clear();
  for (size_t i = 0; i < msgs.size(); ++i) {
    if (last[msgs[i].id] == i) msgs_.push_back(std::move(msgs[i]));
  }
  std::sort(msgs_.begin(), msgs_.end(), before);
  trim();
  ++version_;
}

CircMessage& ChatStore::upsert(CircMessage msg) {
  ++version_;
  auto it = std::find_if(msgs_.begin(), msgs_.end(),
                         [&](const CircMessage& m) { return m.id == msg.id; });
  if (it != msgs_.end()) {
    if (it->timestamp == msg.timestamp) {
      *it = std::move(msg);
      return *it;
    }
    msgs_.erase(it);
  }
  auto pos = std::upper_bound(msgs_.begin(), msgs_.end(), msg, before);
  size_t idx = static_cast<size_t>(pos - msgs_.begin());
  msgs_.insert(pos, std::move(msg));
  if (msgs_.size() > cap_) {
    size_t drop = msgs_.size() - cap_;
    trim();
    // The new message itself may be the oldest; return the oldest survivor.
    idx = idx >= drop ? idx - drop : 0;
  }
  return msgs_[idx];
}

CircMessage* ChatStore::find(const std::string& id) {
  for (auto& m : msgs_) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

bool ChatStore::remove(const std::string& id) {
  auto it = std::find_if(msgs_.begin(), msgs_.end(),
                         [&](const CircMessage& m) { return m.id == id; });
  if (it == msgs_.end()) return false;
  msgs_.erase(it);
  ++version_;
  return true;
}

int64_t ChatStore::newestTimestamp() const {
  return msgs_.empty() ? -1 : msgs_.back().timestamp;
}

int64_t ChatStore::oldestTimestamp() const {
  return msgs_.empty() ? -1 : msgs_.front().timestamp;
}

void ChatStore::trim() {
  while (msgs_.size() > cap_) msgs_.pop_front();
}

}  // namespace rc
