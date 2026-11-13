// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

// The query the API takes: 1–512 characters, counted after trimming.
constexpr size_t kSearchQueryMax = 512;
// How much of a hit's text is kept: its first line, cut to this many bytes.
constexpr size_t kSearchLineMax = 160;

enum class SearchKind { User, Post, Reply };

// One hit of GET /v1/search.
struct SearchHit {
  SearchKind kind = SearchKind::Post;
  std::string id;      // userId, postId or replyId
  std::string postId;  // a reply's entry
  std::string user;    // username, or the author's
  std::string title;   // an entry's title; a person's display name
  std::string line;    // the first non-empty line of the content, plain text
  int64_t createdMs = -1;  // -1 unknown
  int replies = 0;         // an entry's reply count
};

// type=all: up to 8 of each, no paging. False on a malformed body.
bool parseSearchAll(std::string_view body, std::vector<SearchHit>& users, std::vector<SearchHit>& posts,
                    std::vector<SearchHit>& replies);
// One type's page; `cursor` "" when null (the last page).
bool parseSearchPage(std::string_view body, std::vector<SearchHit>& out, std::string& cursor);

// The query trimmed of spaces; empty when there is nothing to search for.
std::string searchQuery(std::string_view typed);
// Whether a trimmed query is one the API takes (1–512 characters).
bool searchQueryOk(std::string_view q);
// The request path: type "all", "posts", "replies" or "users"; page ignored
// for "all".
std::string searchPath(std::string_view q, std::string_view type, int page);
// The API's type name for a kind, as searchPath takes.
const char* searchType(SearchKind k);

}  // namespace rc
