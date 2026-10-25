#include "cs/rtdb.h"

#include <ArduinoJson.h>

#include <cstring>
#include <string_view>
#include <vector>

#include "cs/message_json.h"

namespace rc {

std::string rtdbStreamUrl(const std::string& base, const std::string& path,
                          const std::string& idToken, const RtdbQuery& q) {
  std::string url = base + "/" + path + ".json?auth=" + idToken;
  if (q.orderByTimestamp) url += "&orderBy=%22timestamp%22";
  if (q.startAfter >= 0) url += "&startAfter=" + std::to_string(q.startAfter);
  if (q.limitToLast > 0) url += "&limitToLast=" + std::to_string(q.limitToLast);
  return url;
}

namespace {

// Store adapters used by the shared path logic below.
struct ChatOps {
  ChatStore& s;
  static constexpr bool kWindowPutReplaces = false;
  bool empty() const { return s.size() == 0; }
  void clearAll() { s.clear(); }
  bool remove(const std::string& id) { return s.remove(id); }
  bool exists(const std::string& id) { return s.find(id) != nullptr; }
  void putFresh(const std::string& id, JsonObjectConst o) { s.upsert(messageFromJson(id, o)); }
  void merge(const std::string& id, JsonObjectConst o) {
    CircMessage* cur = s.find(id);
    CircMessage m = cur ? *cur : CircMessage{};
    m.id = id;
    mergeMessageJson(m, o);
    s.upsert(std::move(m));
  }
};

struct PresenceOps {
  PresenceMap& m;
  static constexpr bool kWindowPutReplaces = true;
  bool empty() const { return m.empty(); }
  void clearAll() { m.clear(); }
  bool remove(const std::string& id) { return m.erase(id) > 0; }
  bool exists(const std::string& id) { return m.count(id) > 0; }
  void putFresh(const std::string& id, JsonObjectConst o) {
    PresenceEntry e;
    e.userId = id;
    mergePresenceJson(e, o);
    m[id] = std::move(e);
  }
  void merge(const std::string& id, JsonObjectConst o) {
    PresenceEntry& e = m[id];
    e.userId = id;
    mergePresenceJson(e, o);
  }
};

struct TypingOps {
  TypingMap& m;
  static constexpr bool kWindowPutReplaces = true;
  bool empty() const { return m.empty(); }
  void clearAll() { m.clear(); }
  bool remove(const std::string& id) { return m.erase(id) > 0; }
  bool exists(const std::string& id) { return m.count(id) > 0; }
  void putFresh(const std::string& id, JsonObjectConst o) {
    TypingEntry e;
    e.userId = id;
    mergeTypingJson(e, o);
    m[id] = std::move(e);
  }
  void merge(const std::string& id, JsonObjectConst o) {
    TypingEntry& e = m[id];
    e.userId = id;
    mergeTypingJson(e, o);
  }
};

std::vector<std::string> splitPath(std::string_view p) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < p.size()) {
    size_t j = p.find('/', i);
    if (j == std::string_view::npos) j = p.size();
    if (j > i) out.emplace_back(p.substr(i, j - i));
    i = j + 1;
  }
  return out;
}

// Merges {field: value} into an existing child, if there is one.
template <class Ops>
bool mergeField(Ops& ops, const std::string& id, const std::string& field,
                JsonVariantConst value) {
  if (!ops.exists(id)) return false;
  JsonDocument one;
  one[field] = value;
  ops.merge(id, one.template as<JsonObjectConst>());
  return true;
}

template <class Ops>
StreamResult applyEvent(Ops& ops, const SseEvent& ev) {
  if (ev.event == "keep-alive") return StreamResult::KeepAlive;
  if (ev.event == "cancel") return StreamResult::Cancel;
  if (ev.event == "auth_revoked") return StreamResult::AuthRevoked;
  const bool isPut = ev.event == "put";
  if (!isPut && ev.event != "patch") return StreamResult::Unchanged;

  JsonDocument doc;
  if (deserializeJson(doc, ev.data)) return StreamResult::BadData;
  JsonVariantConst path = doc["path"];
  if (!path.is<const char*>()) return StreamResult::BadData;
  JsonVariantConst data = doc["data"];
  std::vector<std::string> seg = splitPath(path.as<const char*>());

  if (seg.empty()) {
    if (data.isNull()) {
      if (!isPut || !Ops::kWindowPutReplaces || ops.empty()) return StreamResult::Unchanged;
      ops.clearAll();
      return StreamResult::Changed;
    }
    if (!data.is<JsonObjectConst>()) return StreamResult::BadData;
    JsonObjectConst obj = data.as<JsonObjectConst>();
    // Validate everything before touching the store.
    for (JsonPairConst kv : obj) {
      JsonVariantConst v = kv.value();
      bool nested = strchr(kv.key().c_str(), '/') != nullptr;
      if (!nested && !v.is<JsonObjectConst>() && !(v.isNull() && !isPut))
        return StreamResult::BadData;
      if (nested && isPut) return StreamResult::BadData;
    }
    bool changed = false;
    if (isPut && Ops::kWindowPutReplaces && !ops.empty()) {
      ops.clearAll();
      changed = true;
    }
    for (JsonPairConst kv : obj) {
      std::string key = kv.key().c_str();
      JsonVariantConst v = kv.value();
      size_t slash = key.find('/');
      if (slash != std::string::npos) {
        changed |= mergeField(ops, key.substr(0, slash), key.substr(slash + 1), v);
      } else if (v.isNull()) {
        changed |= ops.remove(key);
      } else if (isPut) {
        ops.putFresh(key, v.as<JsonObjectConst>());
        changed = true;
      } else {
        ops.merge(key, v.as<JsonObjectConst>());
        changed = true;
      }
    }
    return changed ? StreamResult::Changed : StreamResult::Unchanged;
  }

  const std::string& id = seg[0];
  if (seg.size() == 1) {
    if (isPut && data.isNull()) {
      return ops.remove(id) ? StreamResult::Changed : StreamResult::Unchanged;
    }
    if (!data.is<JsonObjectConst>()) return StreamResult::BadData;
    if (isPut) ops.putFresh(id, data.as<JsonObjectConst>());
    else ops.merge(id, data.as<JsonObjectConst>());
    return StreamResult::Changed;
  }
  if (seg.size() == 2 && isPut) {
    return mergeField(ops, id, seg[1], data) ? StreamResult::Changed
                                             : StreamResult::Unchanged;
  }
  // Deeper paths carry no fields the device shows.
  return StreamResult::Unchanged;
}

}  // namespace

StreamResult applyChatEvent(ChatStore& store, const SseEvent& ev) {
  ChatOps ops{store};
  return applyEvent(ops, ev);
}

StreamResult applyPresenceEvent(PresenceMap& map, const SseEvent& ev) {
  PresenceOps ops{map};
  return applyEvent(ops, ev);
}

StreamResult applyTypingEvent(TypingMap& map, const SseEvent& ev) {
  TypingOps ops{map};
  return applyEvent(ops, ev);
}

}  // namespace rc
