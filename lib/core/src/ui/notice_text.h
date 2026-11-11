// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "cs/api_parse.h"

namespace rc {

// What Select does with a Feed notice on the Unread screen.
enum class NoticeOpen : uint8_t { None, Entry, Profile };

struct NoticeView {
  NoticeOpen open = NoticeOpen::None;  // None: nothing to list
  std::string text;                    // the row: "@rain followed you"
  std::string user;                    // Profile: whose
};

// A Feed, guild or People notification as an Unread row. The preview (or a
// new entry's title) is cut to previewCodePoints. None for chat mentions,
// post_cooldown, unknown types, an entry notice without a target, and a
// People notice without a sender (absent or "system").
NoticeView describeNotice(const Mention& m, size_t previewCodePoints);

// new_follower, poke, bookmark, new_post_following, new_post_friend: what
// UnreadFilter::Mentions leaves out.
bool isSocialNotice(std::string_view type);

// Which notifications the Unread screen, Home count and light take.
enum class UnreadFilter : uint8_t { All = 0, Mentions = 1 };
// GET /v1/notifications?type=: replies, mentions and guild threads, plus the
// social types for All.
const char* noticeTypes(UnreadFilter f);
UnreadFilter nextUnreadFilter(UnreadFilter f);      // wraps round
UnreadFilter unreadFilterFromIndex(int i);          // out of range is All
const char* unreadFilterKey(UnreadFilter f);        // "all", "mentions": backups and the web page
bool parseUnreadFilter(std::string_view key, UnreadFilter& out);  // false (out unchanged) otherwise
std::u32string unreadFilterLabel(UnreadFilter f);   // U"All", U"Replies & mentions"

}  // namespace rc
