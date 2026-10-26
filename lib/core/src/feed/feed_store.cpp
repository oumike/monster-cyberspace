#include "feed/feed_store.h"

#include <unordered_set>
#include <utility>

#include "feed/markdown_text.h"

namespace rc {

namespace {

void cutPreview(Entry& e) {
  if (truncateCodePoints(e.body, kPreviewChars)) e.truncated = true;
}

const std::string& targetOf(const Bookmark& b) {
  return b.replyId.empty() ? b.postId : b.replyId;
}

}  // namespace

void FeedStore::mergeFirstPage(std::vector<Entry> page, const std::string& cursor) {
  for (auto& e : page) cutPreview(e);
  feedLoaded_ = true;

  bool overlap = false;
  for (const auto& e : page) {
    if (indexOf(e.id) >= 0) {
      overlap = true;
      break;
    }
  }
  if (!overlap) {
    feed_ = std::move(page);
    feedCursor_ = cursor;
    return;
  }

  std::vector<Entry> fresh;
  for (auto& e : page) {
    int i = indexOf(e.id);
    if (i >= 0) {
      feed_[i] = std::move(e);
    } else {
      fresh.push_back(std::move(e));
    }
  }
  feed_.insert(feed_.begin(), std::make_move_iterator(fresh.begin()),
               std::make_move_iterator(fresh.end()));
}

void FeedStore::appendPage(std::vector<Entry> page, const std::string& cursor) {
  feedLoaded_ = true;
  feedCursor_ = cursor;
  std::unordered_set<std::string> held;
  for (const auto& e : feed_) held.insert(e.id);
  for (auto& e : page) {
    if (!held.insert(e.id).second) continue;
    cutPreview(e);
    feed_.push_back(std::move(e));
  }
}

int FeedStore::indexOf(const std::string& postId) const {
  for (size_t i = 0; i < feed_.size(); ++i) {
    if (feed_[i].id == postId) return static_cast<int>(i);
  }
  return -1;
}

void FeedStore::openEntry(Entry e) {
  if (truncateUtf8(e.body, kEntryBodyBytes)) e.truncated = true;
  entry_ = std::make_unique<Entry>(std::move(e));
  replies_.clear();
  repliesCursor_.clear();
  repliesLoaded_ = false;
  watching_ = false;
}

void FeedStore::closeEntry() {
  entry_.reset();
  std::deque<Reply>().swap(replies_);
  repliesCursor_.clear();
  repliesLoaded_ = false;
  watching_ = false;
}

void FeedStore::appendReplies(std::vector<Reply> page, const std::string& cursor) {
  repliesLoaded_ = true;
  repliesCursor_ = cursor;
  std::unordered_set<std::string> held;
  for (const auto& r : replies_) held.insert(r.id);
  for (auto& r : page) {
    if (!held.insert(r.id).second) continue;
    if (truncateUtf8(r.body, kReplyBodyBytes)) r.truncated = true;
    replies_.push_back(std::move(r));
  }
  while (replies_.size() > kMaxReplies) replies_.pop_front();
}

std::string FeedStore::repliesResumeCursor() const {
  if (!repliesCursor_.empty()) return repliesCursor_;
  if (!replies_.empty()) return replies_.back().id;
  return "";
}

void FeedStore::markDeleted(const std::string& id) {
  for (auto& e : feed_) {
    if (e.id == id) e.deleted = true;
  }
  for (auto& e : side_) {
    if (e.id == id) e.deleted = true;
  }
  if (entry_ && entry_->id == id) entry_->deleted = true;
  for (auto& r : replies_) {
    if (r.id == id) r.deleted = true;
  }
}

void FeedStore::setBookmarks(const std::vector<Bookmark>& all) {
  bookmarks_.clear();
  for (const auto& b : all) addBookmark(b);
  bookmarksLoaded_ = true;
}

void FeedStore::addBookmark(const Bookmark& b) {
  const std::string& target = targetOf(b);
  if (target.empty() || b.id.empty()) return;
  bookmarks_[target] = b.id;
}

void FeedStore::removeBookmark(const std::string& targetId) { bookmarks_.erase(targetId); }

std::string FeedStore::bookmarkIdFor(const std::string& targetId) const {
  auto it = bookmarks_.find(targetId);
  return it == bookmarks_.end() ? std::string() : it->second;
}

void FeedStore::setSideList(std::vector<Entry> entries, const std::string& cursor, bool append,
                            std::vector<std::string> replyIds) {
  for (auto& e : entries) cutPreview(e);
  replyIds.resize(entries.size());
  if (!append) {
    side_.clear();
    sideReplyIds_.clear();
  }
  sideReplyIds_.insert(sideReplyIds_.end(), std::make_move_iterator(replyIds.begin()),
                       std::make_move_iterator(replyIds.end()));
  side_.insert(side_.end(), std::make_move_iterator(entries.begin()),
               std::make_move_iterator(entries.end()));
  sideCursor_ = cursor;
}

void FeedStore::clear() {
  std::vector<Entry>().swap(feed_);
  feedCursor_.clear();
  feedLoaded_ = false;
  closeEntry();
  bookmarks_.clear();
  bookmarksLoaded_ = false;
  std::vector<Entry>().swap(side_);
  std::vector<std::string>().swap(sideReplyIds_);
  sideCursor_.clear();
}

}  // namespace rc
