#include "cs/message_json.h"

#include <cstring>

namespace rc {

namespace {

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

void mergeAudio(CircMessage::Audio& a, JsonObjectConst o) {
  for (JsonPairConst kv : o) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "src")) a.src = str(v);
    else if (!strcmp(k, "origin")) a.origin = str(v);
    else if (!strcmp(k, "artist")) a.artist = str(v);
    else if (!strcmp(k, "title")) a.title = str(v);
    else if (!strcmp(k, "genre")) a.genre = str(v);
  }
}

}  // namespace

void mergeMessageJson(CircMessage& m, JsonObjectConst o) {
  bool becameDeleted = false;
  for (JsonPairConst kv : o) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "userId")) m.userId = str(v);
    else if (!strcmp(k, "username")) m.username = str(v);
    else if (!strcmp(k, "content")) m.content = str(v);
    else if (!strcmp(k, "timestamp")) m.timestamp = v.as<int64_t>();
    else if (!strcmp(k, "isChatAdmin")) m.isChatAdmin = v.as<bool>();
    else if (!strcmp(k, "isAction")) m.isAction = v.as<bool>();
    else if (!strcmp(k, "isDice")) m.isDice = v.as<bool>();
    else if (!strcmp(k, "isEightball")) m.isEightball = v.as<bool>();
    else if (!strcmp(k, "eightballAnswer")) m.eightballAnswer = str(v);
    else if (!strcmp(k, "isFortune")) m.isFortune = v.as<bool>();
    else if (!strcmp(k, "fortuneText")) m.fortuneText = str(v);
    else if (!strcmp(k, "imageUrl")) m.imageUrl = str(v);
    else if (!strcmp(k, "gifUrl")) m.gifUrl = str(v);
    else if (!strcmp(k, "deleted")) {
      m.deleted = v.as<bool>();
      becameDeleted = m.deleted;
    } else if (!strcmp(k, "style")) {
      m.styles.clear();
      if (v.is<const char*>()) {
        m.styles.push_back(str(v));
      } else if (v.is<JsonArrayConst>()) {
        for (JsonVariantConst s : v.as<JsonArrayConst>()) {
          if (s.is<const char*>()) m.styles.push_back(str(s));
        }
      }
    } else if (!strcmp(k, "audioAttachment")) {
      m.audio = {};
      m.hasAudio = v.is<JsonObjectConst>();
      if (m.hasAudio) mergeAudio(m.audio, v.as<JsonObjectConst>());
    }
  }
  if (becameDeleted) {
    m.styles.clear();
    m.imageUrl.clear();
    m.gifUrl.clear();
    m.audio = {};
    m.hasAudio = false;
    m.isDice = false;
    m.isEightball = false;
    m.eightballAnswer.clear();
    m.isFortune = false;
    m.fortuneText.clear();
  }
}

CircMessage messageFromJson(const std::string& id, JsonObjectConst o) {
  CircMessage m;
  m.id = id;
  mergeMessageJson(m, o);
  return m;
}

}  // namespace rc
