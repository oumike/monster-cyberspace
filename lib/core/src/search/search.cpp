// SPDX-License-Identifier: GPL-3.0-or-later
#include "search/search.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "feed/feed_parse.h"
#include "feed/markdown_text.h"
#include "util/utf8.h"

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

bool isSpace(char32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == 0xA0; }

std::string firstLine(std::string_view md) {
  std::u32string t = markdownToText(md);
  size_t i = 0;
  while (i < t.size()) {
    size_t e = t.find(U'\n', i);
    if (e == std::u32string::npos) e = t.size();
    size_t a = i, b = e;
    while (a < b && isSpace(t[a])) ++a;
    while (b > a && isSpace(t[b - 1])) --b;
    if (a < b) {
      std::string s = u32ToUtf8(std::u32string_view(t).substr(a, b - a));
      truncateUtf8(s, kSearchLineMax);
      return s;
    }
    i = e + 1;
  }
  return {};
}

int64_t timeOf(JsonVariantConst at) {
  if (at.is<int64_t>()) return at.as<int64_t>();
  if (at.is<const char*>()) {
    int64_t ms = parseIsoMs(str(at));
    return ms > 0 ? ms : -1;
  }
  return -1;
}

bool hitOf(JsonVariantConst d, SearchKind want, bool anyKind, SearchHit& h) {
  std::string type = str(d["type"]);
  SearchKind k = want;
  if (anyKind || !type.empty()) {
    if (type == "user") k = SearchKind::User;
    else if (type == "post") k = SearchKind::Post;
    else if (type == "reply") k = SearchKind::Reply;
    else if (anyKind) return false;
  }
  h = SearchHit{};
  h.kind = k;
  switch (k) {
    case SearchKind::User:
      h.id = str(d["userId"]);
      h.user = str(d["username"]);
      h.title = str(d["displayName"]);
      return !h.user.empty();
    case SearchKind::Post:
      h.id = str(d["postId"]);
      h.user = str(d["authorUsername"]);
      h.title = str(d["title"]);
      h.line = firstLine(str(d["content"]));
      h.replies = d["repliesCount"].is<int>() ? d["repliesCount"].as<int>() : 0;
      h.createdMs = timeOf(d["createdAt"]);
      return !h.id.empty();
    case SearchKind::Reply:
      h.id = str(d["replyId"]);
      h.postId = str(d["postId"]);
      h.user = str(d["authorUsername"]);
      h.line = firstLine(str(d["content"]));
      h.createdMs = timeOf(d["createdAt"]);
      return !h.id.empty() && !h.postId.empty();
  }
  return false;
}

void group(JsonVariantConst arr, SearchKind k, std::vector<SearchHit>& out) {
  out.clear();
  if (!arr.is<JsonArrayConst>()) return;
  SearchHit h;
  for (JsonVariantConst d : arr.as<JsonArrayConst>())
    if (hitOf(d, k, false, h)) out.push_back(std::move(h));
}

}  // namespace

bool parseSearchAll(std::string_view body, std::vector<SearchHit>& users, std::vector<SearchHit>& posts,
                    std::vector<SearchHit>& replies) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size()) || !doc["data"].is<JsonObjectConst>()) return false;
  JsonVariantConst d = doc["data"];
  group(d["users"], SearchKind::User, users);
  group(d["posts"], SearchKind::Post, posts);
  group(d["replies"], SearchKind::Reply, replies);
  return true;
}

bool parseSearchPage(std::string_view body, std::vector<SearchHit>& out, std::string& cursor) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size()) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  SearchHit h;
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>())
    if (hitOf(d, SearchKind::Post, true, h)) out.push_back(std::move(h));
  JsonVariantConst c = doc["cursor"];
  cursor = c.is<const char*>() ? str(c) : c.is<int64_t>() ? std::to_string(c.as<int64_t>()) : std::string();
  return true;
}

std::string searchQuery(std::string_view typed) {
  auto sp = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  size_t a = 0, b = typed.size();
  while (a < b && sp(typed[a])) ++a;
  while (b > a && sp(typed[b - 1])) --b;
  return std::string(typed.substr(a, b - a));
}

bool searchQueryOk(std::string_view q) {
  size_t n = utf8Length(q);
  return n >= 1 && n <= kSearchQueryMax;
}

std::string searchPath(std::string_view q, std::string_view type, int page) {
  static const char* hex = "0123456789ABCDEF";
  std::string p = "/v1/search?q=";
  for (unsigned char c : q) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      p += char(c);
    } else {
      p += '%';
      p += hex[c >> 4];
      p += hex[c & 15];
    }
  }
  p += "&type=";
  p += type;
  if (type != "all") p += "&page=" + std::to_string(std::max(0, page));
  return p;
}

const char* searchType(SearchKind k) {
  switch (k) {
    case SearchKind::User: return "users";
    case SearchKind::Post: return "posts";
    case SearchKind::Reply: return "replies";
  }
  return "posts";
}

}  // namespace rc
