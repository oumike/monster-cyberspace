#include "feed/feed_parse.h"

#include <ArduinoJson.h>

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

bool load(std::string_view body, JsonDocument& doc) {
  return !deserializeJson(doc, body.data(), body.size());
}

bool digits(std::string_view s, size_t pos, size_t n, int& out) {
  if (pos + n > s.size()) return false;
  out = 0;
  for (size_t i = pos; i < pos + n; ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    out = out * 10 + (s[i] - '0');
  }
  return true;
}

// Howard Hinnant's days_from_civil: days since 1970-01-01 in the proleptic Gregorian calendar.
int64_t daysFromCivil(int64_t y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && isLeap(y) ? 29 : kDays[m - 1];
}

std::string cursorOf(JsonVariantConst c) {
  if (c.is<const char*>()) return str(c);
  if (c.is<int64_t>()) return std::to_string(c.as<int64_t>());
  return std::string();
}

std::string attachmentText(JsonVariantConst a) {
  std::string type = str(a["type"]);
  if (type == "audio") {
    std::string artist = str(a["artist"]), title = str(a["title"]);
    if (artist.empty() || title.empty()) return "[song]";
    return "[song] " + artist + " \xE2\x80\x93 " + title;
  }
  if (type == "image") return "[img]";
  return "[attachment]";
}

void attachments(JsonVariantConst arr, std::vector<std::string>& out) {
  out.clear();
  if (!arr.is<JsonArrayConst>()) return;
  for (JsonVariantConst a : arr.as<JsonArrayConst>()) out.push_back(attachmentText(a));
}

void fillEntry(JsonVariantConst d, Entry& e) {
  e = Entry{};
  e.id = str(d["postId"]);
  e.author = str(d["authorUsername"]);
  e.title = str(d["title"]);
  e.slug = str(d["slug"]);
  e.contentWarning = str(d["contentWarning"]);
  e.body = str(d["content"]);
  e.guildSlug = str(d["guildSlug"]);
  if (d["topics"].is<JsonArrayConst>()) {
    for (JsonVariantConst t : d["topics"].as<JsonArrayConst>()) {
      std::string s = str(t);
      if (!s.empty()) e.topics.push_back(std::move(s));
    }
  }
  attachments(d["attachments"], e.attachments);
  e.createdMs = parseIsoMs(str(d["createdAt"]));
  e.replies = d["repliesCount"].as<int>();
  e.isPublic = d["isPublic"].as<bool>();
  e.nsfw = d["isNSFW"].as<bool>();
  e.deleted = d["deleted"].as<bool>();
  e.edited = !d["editedAt"].isNull();
}

void fillReply(JsonVariantConst d, Reply& r) {
  r = Reply{};
  r.id = str(d["replyId"]);
  r.postId = str(d["postId"]);
  r.parentId = str(d["parentReplyId"]);
  r.parentAuthor = str(d["parentReplyAuthor"]);
  r.author = str(d["authorUsername"]);
  r.body = str(d["content"]);
  attachments(d["attachments"], r.attachments);
  r.createdMs = parseIsoMs(str(d["createdAt"]));
  r.deleted = d["deleted"].as<bool>();
  r.edited = !d["editedAt"].isNull();
}

// Loads a {"data":[...],"cursor":...} list body.
bool loadList(std::string_view body, JsonDocument& doc, std::string& cursor) {
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  cursor = cursorOf(doc["cursor"]);
  return true;
}

bool loadObject(std::string_view body, JsonDocument& doc) {
  return load(body, doc) && doc["data"].is<JsonObjectConst>();
}

}  // namespace

int64_t parseIsoMs(std::string_view s) {
  int y, mo, d, h, mi, se;
  if (s.size() < 20 || !digits(s, 0, 4, y) || s[4] != '-' || !digits(s, 5, 2, mo) || s[7] != '-' ||
      !digits(s, 8, 2, d) || s[10] != 'T' || !digits(s, 11, 2, h) || s[13] != ':' ||
      !digits(s, 14, 2, mi) || s[16] != ':' || !digits(s, 17, 2, se)) {
    return -1;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, mo) || h > 23 || mi > 59 || se > 60) return -1;
  size_t pos = 19;
  int ms = 0;
  if (s[pos] == '.') {
    ++pos;
    int scale = 100, n = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
      ms += (s[pos] - '0') * scale;
      scale /= 10;
      ++pos;
      ++n;
    }
    if (n == 0) return -1;
  }
  if (pos + 1 != s.size() || s[pos] != 'Z') return -1;
  int64_t secs = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se;
  return secs * 1000 + ms;
}

bool parseFeedPage(std::string_view body, std::vector<Entry>& out, std::string& cursor) {
  JsonDocument doc;
  if (!loadList(body, doc, cursor)) return false;
  out.clear();
  for (JsonVariantConst v : doc["data"].as<JsonArrayConst>()) {
    Entry e;
    fillEntry(v, e);
    out.push_back(std::move(e));
  }
  return true;
}

bool parseEntry(std::string_view body, Entry& out) {
  JsonDocument doc;
  if (!loadObject(body, doc)) return false;
  fillEntry(doc["data"], out);
  return !out.id.empty();
}

bool parseReplies(std::string_view body, std::vector<Reply>& out, std::string& cursor) {
  JsonDocument doc;
  if (!loadList(body, doc, cursor)) return false;
  out.clear();
  for (JsonVariantConst v : doc["data"].as<JsonArrayConst>()) {
    Reply r;
    fillReply(v, r);
    out.push_back(std::move(r));
  }
  return true;
}

bool parseReply(std::string_view body, Reply& out) {
  JsonDocument doc;
  if (!loadObject(body, doc)) return false;
  fillReply(doc["data"], out);
  return !out.id.empty();
}

bool parseCreatedId(std::string_view body, std::string& id) {
  JsonDocument doc;
  if (!loadObject(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  for (const char* key : {"postId", "replyId", "bookmarkId"}) {
    std::string v = str(d[key]);
    if (!v.empty()) {
      id = std::move(v);
      return true;
    }
  }
  return false;
}

bool parseBookmarks(std::string_view body, std::vector<Bookmark>& out, std::string& cursor) {
  JsonDocument doc;
  if (!loadList(body, doc, cursor)) return false;
  out.clear();
  for (JsonVariantConst v : doc["data"].as<JsonArrayConst>()) {
    out.push_back(Bookmark{str(v["bookmarkId"]), str(v["postId"]), str(v["replyId"])});
  }
  return true;
}

bool parseWatches(std::string_view body, std::vector<std::string>& postIds, std::string& cursor) {
  JsonDocument doc;
  if (!loadList(body, doc, cursor)) return false;
  postIds.clear();
  for (JsonVariantConst v : doc["data"].as<JsonArrayConst>()) {
    std::string id = str(v["postId"]);
    if (!id.empty()) postIds.push_back(std::move(id));
  }
  return true;
}

bool parseWatching(std::string_view body, bool& watching) {
  JsonDocument doc;
  if (!loadObject(body, doc) || !doc["data"]["watching"].is<bool>()) return false;
  watching = doc["data"]["watching"].as<bool>();
  return true;
}

}  // namespace rc
