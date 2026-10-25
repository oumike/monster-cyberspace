#include "cs/api_parse.h"

#include <ArduinoJson.h>

#include <cstdlib>

#include "cs/message_json.h"

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

bool load(std::string_view body, JsonDocument& doc) {
  return !deserializeJson(doc, body.data(), body.size());
}

}  // namespace

ApiError parseError(int http, std::string_view body) {
  ApiError e;
  e.http = http;
  JsonDocument doc;
  if (load(body, doc)) {
    JsonVariantConst err = doc["error"];
    if (err.is<JsonObjectConst>()) {
      e.code = str(err["code"]);
      e.message = str(err["message"]);
    }
  }
  if (!e.ok() && e.code.empty()) {
    e.code = "HTTP_" + std::to_string(http);
    if (e.message.empty()) e.message = "Server error";
  }
  return e;
}

bool parseTokens(std::string_view body, Tokens& out) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  std::string id = str(d["idToken"]), url = str(d["rtdbUrl"]);
  if (id.empty() || url.empty()) return false;
  out.idToken = std::move(id);
  out.rtdbUrl = std::move(url);
  std::string refresh = str(d["refreshToken"]);
  if (!refresh.empty()) out.refreshToken = std::move(refresh);
  return true;
}

bool parseRooms(std::string_view body, std::vector<RoomInfo>& out) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst r : doc["data"].as<JsonArrayConst>()) {
    RoomInfo info;
    info.id = str(r["id"]);
    info.slug = str(r["slug"]);
    info.name = str(r["name"]);
    info.lastMessageAt = r["lastMessageAt"].as<int64_t>();
    info.sortOrder = r["sortOrder"].as<int>();
    info.onlineCount = r["onlineCount"].as<int>();
    out.push_back(std::move(info));
  }
  return true;
}

bool parseHistory(std::string_view body, std::vector<CircMessage>& out, int64_t& cursor) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst m : doc["data"].as<JsonArrayConst>()) {
    if (!m.is<JsonObjectConst>()) continue;
    out.push_back(messageFromJson(str(m["id"]), m.as<JsonObjectConst>()));
  }
  JsonVariantConst c = doc["cursor"];
  if (c.is<const char*>()) {
    char* end = nullptr;
    const char* s = c.as<const char*>();
    long long v = strtoll(s, &end, 10);
    cursor = (end != s && *end == '\0') ? v : -1;
  } else if (c.is<int64_t>() || c.is<double>()) {
    cursor = c.as<int64_t>();
  } else {
    cursor = -1;
  }
  return true;
}

bool parseUsers(std::string_view body, std::vector<PresenceEntry>& out) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst u : doc["data"].as<JsonArrayConst>()) {
    if (!u.is<JsonObjectConst>()) continue;
    PresenceEntry e;
    mergePresenceJson(e, u.as<JsonObjectConst>());
    e.online = true;
    out.push_back(std::move(e));
  }
  return true;
}

bool parsePresenceAck(std::string_view body, PresenceCadence& out) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d.is<JsonObjectConst>()) return false;
  if (d["heartbeatMs"].is<int64_t>()) out.heartbeatMs = d["heartbeatMs"].as<int64_t>();
  if (d["staleAfterMs"].is<int64_t>()) out.staleAfterMs = d["staleAfterMs"].as<int64_t>();
  if (d["idleAfterMs"].is<int64_t>()) out.idleAfterMs = d["idleAfterMs"].as<int64_t>();
  return true;
}

bool parseSendResult(std::string_view body, std::string& messageId, std::string& reply) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d.is<JsonObjectConst>()) return false;
  messageId = str(d["messageId"]);
  reply = str(d["reply"]);
  return true;
}

bool parseMentions(std::string_view body, std::vector<Mention>& out) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst n : doc["data"].as<JsonArrayConst>()) {
    Mention m;
    m.id = str(n["id"]);
    m.actorUsername = str(n["actorUsername"]);
    JsonVariantConst md = n["metadata"];
    m.roomSlug = str(md["roomSlug"]);
    m.roomName = str(md["roomName"]);
    m.messageContent = str(md["messageContent"]);
    out.push_back(std::move(m));
  }
  return true;
}

bool parseUnreadCount(std::string_view body, int& count, bool& exact) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d["count"].is<int>()) return false;
  count = d["count"].as<int>();
  exact = d["exact"] | true;
  return true;
}

}  // namespace rc
