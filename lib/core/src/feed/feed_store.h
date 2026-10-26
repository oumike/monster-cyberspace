#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "feed/entry.h"

namespace rc {

constexpr size_t kPreviewChars = 280, kEntryBodyBytes = 16384, kReplyBodyBytes = 4096;
constexpr size_t kMaxReplies = 100;

// The feed list, the open entry with its replies, the bookmark map and the
// Watched/Bookmarks side list. References returned here are invalidated by the
// next mutation.
class FeedStore {
 public:
  // Feed list. Bodies are cut to kPreviewChars (truncated=true when cut).
  // First page: if any id is already held, new ids go on top in page order
  // and held ones are updated in place, keeping the old cursor; with no
  // overlap the list is replaced and the cursor taken from the page.
  void mergeFirstPage(std::vector<Entry> page, const std::string& cursor);
  void appendPage(std::vector<Entry> page, const std::string& cursor);  // skips held ids
  const std::vector<Entry>& feed() const { return feed_; }
  bool feedEnd() const { return feedLoaded_ && feedCursor_.empty(); }
  const std::string& feedCursor() const { return feedCursor_; }
  int indexOf(const std::string& postId) const;  // -1 when absent

  // Open entry (body cut to kEntryBodyBytes) and its replies.
  void openEntry(Entry e);  // clears replies, cursor and watching
  void closeEntry();        // frees the entry and its replies
  const Entry* entry() const { return entry_.get(); }
  // Skips held ids and cuts bodies to kReplyBodyBytes. Over kMaxReplies,
  // drops the oldest.
  void appendReplies(std::vector<Reply> page, const std::string& cursor);
  const std::deque<Reply>& replies() const { return replies_; }
  bool repliesEnd() const { return repliesLoaded_ && repliesCursor_.empty(); }
  // The cursor if set, else the newest held reply's id ("" when none).
  std::string repliesResumeCursor() const;
  void setWatching(bool w) { watching_ = w; }
  bool watching() const { return watching_; }
  void markDeleted(const std::string& id);  // an entry (feed or open) or a reply

  // Bookmarks: post or reply id → bookmarkId.
  void setBookmarks(const std::vector<Bookmark>& all);  // sets bookmarksLoaded
  void addBookmark(const Bookmark& b);
  void removeBookmark(const std::string& targetId);
  std::string bookmarkIdFor(const std::string& targetId) const;  // "" when none
  bool bookmarksLoaded() const { return bookmarksLoaded_; }

  // Watched and Bookmarks screens: resolved entries, previews like the feed.
  void setSideList(std::vector<Entry> entries, const std::string& cursor, bool append);
  const std::vector<Entry>& sideList() const { return side_; }
  const std::string& sideCursor() const { return sideCursor_; }

  void clear();  // on sign-out: everything above, plus bookmarksLoaded=false

 private:
  std::vector<Entry> feed_;
  std::string feedCursor_;
  bool feedLoaded_ = false;

  std::unique_ptr<Entry> entry_;
  std::deque<Reply> replies_;
  std::string repliesCursor_;
  bool repliesLoaded_ = false;
  bool watching_ = false;

  std::map<std::string, std::string> bookmarks_;  // target id → bookmarkId
  bool bookmarksLoaded_ = false;

  std::vector<Entry> side_;
  std::string sideCursor_;
};

}  // namespace rc
