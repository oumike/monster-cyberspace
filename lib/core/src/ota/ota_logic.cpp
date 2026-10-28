#include "ota/ota_logic.h"

#include <cctype>

namespace rc {

namespace {

constexpr const char* kDownloadBase = "https://github.com/oumike/monster-cyberspace/releases/download/";

bool digit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

// The next number before any '-' or '+', or -1 at the end.
long nextPart(const char*& p) {
  while (*p && *p != '-' && *p != '+' && !digit(*p)) ++p;
  if (!*p || *p == '-' || *p == '+') return -1;
  long v = 0;
  while (digit(*p)) v = v * 10 + (*p++ - '0');
  return v;
}

// v<digits>.<digits>.<digits> and nothing else.
bool plainTag(const std::string& t) {
  if (t.size() < 6 || t.size() > 32 || t[0] != 'v') return false;
  int dots = 0;
  bool needDigit = true;
  for (size_t i = 1; i < t.size(); ++i) {
    if (digit(t[i])) {
      needDigit = false;
    } else if (t[i] == '.' && !needDigit) {
      ++dots;
      needDigit = true;
    } else {
      return false;
    }
  }
  return dots == 2 && !needDigit;
}

}  // namespace

int compareVersions(const char* a, const char* b) {
  while (true) {
    long va = nextPart(a), vb = nextPart(b);
    if (va < 0 && vb < 0) return 0;
    if (va < 0) va = 0;
    if (vb < 0) vb = 0;
    if (va != vb) return va < vb ? -1 : 1;
  }
}

std::string tagFromLocation(const std::string& location) {
  static const std::string kMark = "/releases/tag/";
  const size_t k = location.rfind(kMark);
  if (k == std::string::npos) return "";
  const std::string tag = location.substr(k + kMark.size());
  return plainTag(tag) ? tag : "";
}

std::string otaAssetName(const std::string& slug, const std::string& tag) {
  return "monster-cyberspace-" + slug + "-" + tag + "-ota.bin";
}

std::string otaAssetUrl(const std::string& slug, const std::string& tag) {
  return kDownloadBase + tag + "/" + otaAssetName(slug, tag);
}

}  // namespace rc
