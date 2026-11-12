// SPDX-License-Identifier: GPL-3.0-or-later
#include "programs/programs.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "feed/feed_parse.h"
#include "feed/markdown_text.h"

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

ProgramInfo infoOf(JsonVariantConst d) {
  ProgramInfo p;
  p.id = str(d["id"]);
  p.name = str(d["name"]);
  p.owner = str(d["ownerUsername"]);
  p.description = str(d["description"]);
  p.runtime = str(d["runtime"]);
  if (p.runtime.empty()) p.runtime = "web";
  p.release = d["release"].is<int>() ? d["release"].as<int>() : 0;
  JsonVariantConst at = d["publishedAt"];
  if (at.is<int64_t>()) p.publishedMs = at.as<int64_t>();
  else if (at.is<const char*>()) p.publishedMs = std::max<int64_t>(0, parseIsoMs(str(at)));
  return p;
}

}  // namespace

bool parseProgramPage(std::string_view body, std::vector<ProgramInfo>& out, std::string& cursor) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size()) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    ProgramInfo p = infoOf(d);
    if (!p.id.empty()) out.push_back(std::move(p));
  }
  JsonVariantConst c = doc["cursor"];
  cursor = c.is<const char*>() ? str(c) : c.is<int64_t>() ? std::to_string(c.as<int64_t>()) : std::string();
  return true;
}

size_t base64DecodedSize(std::string_view b64) {
  size_t n = 0, pad = 0;
  for (char c : b64) {
    if (c == '=') ++pad;
    else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') ++n;
  }
  pad = std::min<size_t>(pad, 2);
  return pad ? (n + pad) / 4 * 3 - pad : n * 3 / 4;
}

bool parseProgramSource(std::string_view body, ProgramSource& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body.data(), body.size())) return false;
  JsonVariantConst d = doc.as<JsonVariantConst>();
  if (d["data"].is<JsonObjectConst>()) d = d["data"];
  ProgramSource s;
  s.info = infoOf(d);
  if (s.info.id.empty()) return false;
  const char* src = d["source"].as<const char*>();
  std::string_view text = src ? std::string_view(src) : std::string_view();
  s.binary = str(d["encoding"]) == "base64";
  if (s.binary) {
    s.bytes = base64DecodedSize(text);
  } else {
    s.bytes = text.size();
    s.text.assign(text.substr(0, std::min(text.size(), kSourceShowMax + 4)));
    s.cut = truncateUtf8(s.text, kSourceShowMax) || text.size() > s.text.size();
  }
  out = std::move(s);
  return true;
}

std::u32string cleanSource(std::u32string_view text) {
  std::u32string out;
  out.reserve(text.size());
  for (char32_t c : text) {
    if (c == U'\n') out += c;
    else if (c == U'\t') out += U"  ";
    else if (c >= 0x20 && c != 0x7F) out += c;
  }
  if (!out.empty() && out.back() == U'\n') out.pop_back();
  return out;
}

}  // namespace rc
