#include "ui/kb_light.h"

namespace rc {

namespace {
constexpr uint8_t kDuty[kKbLightCount] = {0, 64, 160, 255};
constexpr const char* kKeys[kKbLightCount] = {"off", "low", "medium", "high"};
constexpr const char32_t* kLabels[kKbLightCount] = {U"Off", U"Low", U"Medium", U"High"};
int idx(KbLight l) {
  const int i = static_cast<int>(l);
  return i >= 0 && i < kKbLightCount ? i : 0;
}
}  // namespace

uint8_t kbLightDuty(KbLight l) { return kDuty[idx(l)]; }
const char* kbLightKey(KbLight l) { return kKeys[idx(l)]; }
const char32_t* kbLightLabel(KbLight l) { return kLabels[idx(l)]; }

KbLight kbLightFromIndex(int i, KbLight fallback) {
  return i >= 0 && i < kKbLightCount ? static_cast<KbLight>(i) : fallback;
}

bool parseKbLight(std::string_view key, KbLight& out) {
  for (int i = 0; i < kKbLightCount; ++i)
    if (key == kKeys[i]) {
      out = static_cast<KbLight>(i);
      return true;
    }
  return false;
}

}  // namespace rc
