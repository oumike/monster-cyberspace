#include "guild/guild_parse.h"

#include <ArduinoJson.h>

#include "feed/feed_parse.h"

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

bool load(std::string_view body, JsonDocument& doc) {
  return !deserializeJson(doc, body.data(), body.size());
}

std::string cursorOf(JsonVariantConst c) {
  if (c.is<const char*>()) return str(c);
  if (c.is<int64_t>()) return std::to_string(c.as<int64_t>());
  return std::string();
}

void fillGuild(JsonVariantConst d, Guild& g) {
  g = Guild{};
  g.id = str(d["id"]);
  g.slug = str(d["slug"]);
  g.name = str(d["name"]);
  g.icon = str(d["icon"]);
  g.founder = str(d["founderUsername"]);
  g.bio = str(d["bio"]);
  g.link = str(d["link"]);
  g.linkText = str(d["linkText"]);
  g.role = str(d["role"]);
  g.createdMs = parseIsoMs(str(d["createdAt"]));
  g.members = d["memberCount"].as<int>();
  g.apprentices = d["apprenticeCount"].as<int>();
}

}  // namespace

bool parseGuildPage(std::string_view body, std::vector<Guild>& out, std::string& cursor) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    if (!d.is<JsonObjectConst>()) continue;
    Guild g;
    fillGuild(d, g);
    out.push_back(std::move(g));
  }
  cursor = cursorOf(doc["cursor"]);
  return true;
}

bool parseGuild(std::string_view body, Guild& out) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d.is<JsonObjectConst>()) return false;
  fillGuild(d, out);
  return true;
}

bool parseMyGuilds(std::string_view body, std::vector<MyGuild>& out) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    if (!d.is<JsonObjectConst>()) continue;
    out.push_back({str(d["slug"]), str(d["name"]), str(d["icon"]), str(d["role"])});
  }
  return true;
}

bool parseMembers(std::string_view body, std::vector<Member>& out, std::string& cursor) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    if (!d.is<JsonObjectConst>()) continue;
    Member m;
    m.id = str(d["membershipId"]);
    m.username = str(d["username"]);
    m.role = str(d["role"]);
    m.joinedMs = parseIsoMs(str(d["joinedAt"]));
    out.push_back(std::move(m));
  }
  cursor = cursorOf(doc["cursor"]);
  return true;
}

bool parseGuildRole(std::string_view body, std::string& role) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d.is<JsonObjectConst>() || !d["role"].is<const char*>()) return false;
  role = str(d["role"]);
  return true;
}

}  // namespace rc
