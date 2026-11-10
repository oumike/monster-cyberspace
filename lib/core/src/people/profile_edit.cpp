#include "people/profile_edit.h"

#include <ArduinoJson.h>

namespace rc {

namespace {

size_t chars(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) ++n;
  return n;
}

std::string trimmed(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return std::string();
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

void put(JsonDocument& doc, const char* key, const std::string& before, const std::string& after) {
  if (before == after) return;
  std::string t = trimmed(after);
  if (t.empty()) {
    if (trimmed(before).empty()) return;
    doc[key] = nullptr;
  } else {
    doc[key] = after;
  }
}

}  // namespace

ProfileFields profileFields(const Profile& p) {
  return ProfileFields{p.displayName, p.bio, p.websiteUrl, p.websiteName, p.locationName};
}

std::string profilePatchJson(const ProfileFields& b, const ProfileFields& a) {
  JsonDocument doc;
  doc.to<JsonObject>();
  put(doc, "displayName", b.displayName, a.displayName);
  put(doc, "bio", b.bio, a.bio);
  put(doc, "websiteUrl", b.websiteUrl, a.websiteUrl);
  put(doc, "websiteName", b.websiteName, a.websiteName);
  put(doc, "locationName", b.locationName, a.locationName);
  if (doc.as<JsonObjectConst>().size() == 0) return std::string();
  std::string out;
  serializeJson(doc, out);
  return out;
}

const char* profileFieldError(const ProfileFields& f) {
  if (chars(f.displayName) > 64) return "Display name: 64 characters at most";
  if (chars(f.bio) > 640) return "Bio: 640 characters at most";
  std::string url = trimmed(f.websiteUrl);
  if (!url.empty()) {
    if (chars(url) > 2048) return "Website: 2048 characters at most";
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
      return "Website must start with http:// or https://";
  }
  if (chars(f.websiteName) > 64) return "Website name: 64 characters at most";
  if (chars(f.locationName) > 64) return "Location: 64 characters at most";
  return nullptr;
}

}  // namespace rc
