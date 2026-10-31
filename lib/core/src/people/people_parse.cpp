#include "people/people_parse.h"

#include <ArduinoJson.h>

#include "feed/feed_parse.h"
#include "feed/markdown_text.h"

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

int count(JsonVariantConst v) { return v.is<int>() ? v.as<int>() : -1; }

}  // namespace

bool parseProfile(std::string_view body, Profile& out) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  JsonVariantConst d = doc["data"];
  if (!d.is<JsonObjectConst>()) return false;
  Profile p;
  p.username = str(d["username"]);
  if (p.username.empty()) return false;
  p.userId = str(d["userId"]);
  p.displayName = str(d["displayName"]);
  p.bio = str(d["bio"]);
  truncateUtf8(p.bio, kProfileBioBytes);
  p.guildSlug = str(d["guildSlug"]);
  p.guildIcon = str(d["guildIcon"]);
  p.guildName = str(d["guildName"]);
  p.locationName = str(d["locationName"]);
  p.websiteUrl = str(d["websiteUrl"]);
  p.websiteName = str(d["websiteName"]);
  p.serial = count(d["serialNumber"]);
  p.followers = count(d["followersCount"]);
  p.following = count(d["followingCount"]);
  p.publicPosts = count(d["publicPostsCount"]);
  std::string created = str(d["createdAt"]);
  std::string active = str(d["lastActiveAt"]);
  p.createdMs = created.empty() ? -1 : parseIsoMs(created);
  p.lastActiveMs = active.empty() ? -1 : parseIsoMs(active);
  p.supporter = d["isSupporter"].as<bool>();
  out = std::move(p);
  return true;
}

bool parseBlockedUsers(std::string_view body, std::vector<std::string>& out) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonObjectConst>()) return false;
  out.clear();
  for (JsonVariantConst u : doc["data"]["blockedUsers"].as<JsonArrayConst>()) {
    std::string s = str(u);
    if (!s.empty()) out.push_back(std::move(s));
  }
  return true;
}

bool parseFollowPage(std::string_view body, std::vector<Follow>& out, std::string& cursor) {
  JsonDocument doc;
  if (!load(body, doc) || !doc["data"].is<JsonArrayConst>()) return false;
  out.clear();
  for (JsonVariantConst d : doc["data"].as<JsonArrayConst>()) {
    Follow f{str(d["followId"]), str(d["followerId"]), str(d["followedId"])};
    if (!f.followId.empty() && !f.followedId.empty()) out.push_back(std::move(f));
  }
  cursor = cursorOf(doc["cursor"]);
  return true;
}

bool parseFollowId(std::string_view body, std::string& followId) {
  JsonDocument doc;
  if (!load(body, doc)) return false;
  std::string id = str(doc["data"]["followId"]);
  if (id.empty()) return false;
  followId = std::move(id);
  return true;
}

}  // namespace rc
