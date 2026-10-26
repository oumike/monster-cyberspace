#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "feed/entry.h"
#include "ui/grid.h"
#include "ui/room_layout.h"
#include "ui/theme.h"

namespace rc {

// "now", "5m", "3h", "2d", or the local date "25 Oct" once ≥ 7 days.
std::string relativeAge(int64_t createdMs, int64_t nowMs);

struct FeedRow {
  std::u32string line1, line2;
  uint16_t fg1, fg2;
};

// line1: "nick · 3h · ↩5" (+ " ★" when bookmarked); line2 dim: the title, else the
// first non-empty line of markdownToText(body); with a cw: "CW: <cw>"; nsfw only:
// "CW: NSFW"; deleted: "[deleted]". Both lines are cut to cols.
FeedRow layoutFeedRow(const Entry& e, bool bookmarked, int64_t nowMs, int cols, const Theme& t);

struct EntryLayoutOptions {
  int cols = 53;
  TextSize size = TextSize::Compact;
  int64_t nowMs = 0;
  std::string self;
  bool revealed = false;  // the CW/NSFW body is shown
  const Theme* theme = nullptr;
};

// Header lines (messageId = entry id): "author · 3h", then "#t1 #t2 · public · NSFW".
// Body lines (messageId = entry id): the wrapped markdownToText(body), or "CW: …" until
// revealed; then the attachment lines; then "… read the rest on the web" if truncated.
// A divider line "── N replies ──" (divider = true). Each reply: an optional dim
// "↳ @parentAuthor" line, then the reply in the room log style for o.size
// (messageId = reply id). Deleted bodies render as a dim "[deleted]".
std::vector<DisplayLine> layoutEntry(const Entry& e, const std::deque<Reply>& replies,
                                     const EntryLayoutOptions& o);

}  // namespace rc
