#include "web/compose_req.h"

#include <ArduinoJson.h>

#include "feed/compose.h"

namespace rc {

namespace {

std::string trimmed(std::string_view s) {
  const char* ws = " \t\r\n";
  const size_t a = s.find_first_not_of(ws);
  if (a == std::string_view::npos) return "";
  return std::string(s.substr(a, s.find_last_not_of(ws) - a + 1));
}

size_t codePoints(std::string_view s) {
  size_t n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) ++n;
  return n;
}

bool optString(JsonVariantConst v, std::string& out) {
  if (v.isNull()) return true;
  if (!v.is<const char*>()) return false;
  out = v.as<const char*>();
  return true;
}

}  // namespace

std::string parseCompose(std::string_view json, ComposeReq& r) {
  r = ComposeReq{};
  JsonDocument doc;
  if (deserializeJson(doc, json.data(), json.size()) || !doc.is<JsonObjectConst>()) return "Bad request";
  const std::string_view kind = doc["kind"] | "";
  if (kind == "open") r.kind = ComposeKind::Open;
  else if (kind == "mail") r.kind = ComposeKind::Mail;
  else if (kind == "thread") r.kind = ComposeKind::Thread;
  else return "Bad request";
  if (!optString(doc["text"], r.text) || !optString(doc["to"], r.to) ||
      !optString(doc["guild"], r.guild) || !optString(doc["title"], r.title))
    return "Bad request";
  return "";
}

std::string checkCompose(ComposeReq& r, bool openTakesText) {
  // Chat lines are one line; a thread's body keeps its paragraphs.
  if (r.kind != ComposeKind::Thread)
    for (char& c : r.text)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  r.text = trimmed(r.text);
  if (r.text.empty()) return "Text is empty";
  if (codePoints(r.text) > kBodyMaxChars) return "Text over 4096 characters";
  switch (r.kind) {
    case ComposeKind::Open:
      if (!openTakesText) return "Nothing open on the device takes text";
      break;
    case ComposeKind::Mail: {
      std::string to = trimmed(r.to);
      if (!to.empty() && to.front() == '@') to = trimmed(to.substr(1));
      if (to.empty()) return "Recipient is empty";
      for (char c : to)
        if (c == ' ' || c == '\t' || c == '/' || static_cast<unsigned char>(c) < 0x20)
          return "Recipient: a user name, no spaces";
      r.to = to;
      break;
    }
    case ComposeKind::Thread: {
      r.guild = trimmed(r.guild);
      if (r.guild.empty()) return "Guild is empty";
      if (r.guild.find_first_of(" /\t") != std::string::npos) return "Guild: its short name, no spaces";
      r.title = trimmed(r.title);
      EntryDraft d;
      d.title = r.title;
      d.body = r.text;
      std::string err = validateDraft(d);
      if (!err.empty()) return err;
      break;
    }
  }
  return "";
}

}  // namespace rc
