#include "notes/notes.h"

#include <ArduinoJson.h>

#include "feed/feed_parse.h"
#include "feed/markdown_text.h"

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

int64_t timeOf(JsonVariantConst v) {
  std::string s = str(v);
  return s.empty() ? -1 : parseIsoMs(s);
}

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

}  // namespace

std::string noteTitle(std::string_view c) {
  size_t pos = 0;
  while (pos <= c.size()) {
    size_t end = c.find('\n', pos);
    if (end == std::string_view::npos) end = c.size();
    size_t a = pos, b = end;
    while (a < b && blank(c[a])) ++a;
    while (b > a && blank(c[b - 1])) --b;
    if (a < b && c[a] == '#') {
      while (a < b && c[a] == '#') ++a;
      while (a < b && blank(c[a])) ++a;
    }
    if (a < b) {
      std::string t(c.substr(a, b - a));
      truncateUtf8(t, kNoteTitleBytes);
      return t;
    }
    pos = end + 1;
  }
  return std::string();
}

bool parseNoteList(std::string_view body, std::vector<NoteRow>& out, std::string& cursor) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size()) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    if (d["deleted"].as<bool>()) continue;
    NoteRow r;
    r.id = str(d["noteId"]);
    if (r.id.empty()) continue;
    const char* content = d["content"].as<const char*>();
    r.title = noteTitle(content ? std::string_view(content) : std::string_view());
    r.updatedMs = timeOf(d["updatedAt"]);
    r.revision = d["revisionNumber"].is<int>() ? d["revisionNumber"].as<int>() : 0;
    out.push_back(std::move(r));
  }
  JsonVariantConst c = doc["cursor"];
  cursor = c.is<const char*>() ? str(c) : c.is<int64_t>() ? std::to_string(c.as<int64_t>()) : std::string();
  return true;
}

bool parseNote(std::string_view body, Note& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size())) return false;
  JsonVariantConst d = doc["data"];
  Note n;
  n.id = str(d["noteId"]);
  if (n.id.empty()) return false;
  n.content = str(d["content"]);
  n.updatedMs = timeOf(d["updatedAt"]);
  n.revision = d["revisionNumber"].is<int>() ? d["revisionNumber"].as<int>() : 0;
  out = std::move(n);
  return true;
}

bool parseNoteId(std::string_view body, std::string& id) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size())) return false;
  id = str(doc["data"]["noteId"]);
  return !id.empty();
}

std::string noteBodyJson(const std::string& content) {
  JsonDocument doc;
  doc["content"] = content;
  std::string out;
  serializeJson(doc, out);
  return out;
}

const char* noteError(const std::string& content) {
  if (content.find_first_not_of(" \t\r\n") == std::string::npos) return "Note is empty";
  size_t n = 0;
  for (unsigned char c : content)
    if ((c & 0xC0) != 0x80) ++n;
  return n > kNoteMaxChars ? "Note is too long" : nullptr;
}

}  // namespace rc
