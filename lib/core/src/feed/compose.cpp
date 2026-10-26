#include "feed/compose.h"

#include <ArduinoJson.h>

namespace rc {

namespace {

constexpr size_t kTitleMaxChars = 100;
constexpr size_t kMaxTopics = 3;
constexpr size_t kWarningMaxChars = 60;

bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
  while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
  return s;
}

size_t codePoints(std::string_view s) {
  size_t n = 0;
  for (char c : s) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
  }
  return n;
}

std::string serialize(const JsonDocument& doc) {
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace

std::vector<std::string> splitTopics(std::string_view s) {
  std::vector<std::string> out;
  while (true) {
    size_t comma = s.find(',');
    std::string_view part = trim(s.substr(0, comma));
    if (!part.empty() && part.front() == '#') part = trim(part.substr(1));
    if (!part.empty()) {
      std::string t(part);
      for (char& c : t) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      }
      out.push_back(std::move(t));
    }
    if (comma == std::string_view::npos) break;
    s.remove_prefix(comma + 1);
  }
  return out;
}

std::string validateDraft(const EntryDraft& d) {
  if (trim(d.body).empty()) return "Body is empty";
  if (codePoints(d.body) > kBodyMaxChars) return "Body over 4096 characters";
  if (codePoints(d.title) > kTitleMaxChars) return "Title over 100 characters";
  if (splitTopics(d.topics).size() > kMaxTopics) return "At most 3 topics";
  if (codePoints(d.cw) > kWarningMaxChars) return "Warning over 60 characters";
  return "";
}

std::string entryJson(const EntryDraft& d) {
  JsonDocument doc;
  doc["content"] = d.body;
  if (!d.title.empty()) doc["title"] = d.title;
  std::vector<std::string> topics = splitTopics(d.topics);
  if (!topics.empty()) {
    JsonArray arr = doc["topics"].to<JsonArray>();
    for (const std::string& t : topics) arr.add(t);
  }
  if (d.isPublic) doc["isPublic"] = true;
  if (d.nsfw) doc["isNSFW"] = true;
  if (!d.cw.empty()) doc["contentWarning"] = d.cw;
  return serialize(doc);
}

std::string replyJson(const std::string& postId, const std::string& body,
                      const std::string& parentReplyId) {
  JsonDocument doc;
  doc["postId"] = postId;
  doc["content"] = body;
  if (!parentReplyId.empty()) doc["parentReplyId"] = parentReplyId;
  return serialize(doc);
}

std::string flagJson(const std::string& reason) {
  JsonDocument doc;
  doc.to<JsonObject>();
  if (!reason.empty()) doc["reason"] = reason;
  return serialize(doc);
}

std::string bookmarkJson(const std::string& postId, const std::string& replyId) {
  JsonDocument doc;
  if (!postId.empty()) {
    doc["postId"] = postId;
    doc["type"] = "post";
  } else {
    doc["replyId"] = replyId;
    doc["type"] = "reply";
  }
  return serialize(doc);
}

}  // namespace rc
