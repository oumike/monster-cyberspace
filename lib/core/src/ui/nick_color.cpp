#include "ui/nick_color.h"

#include <cctype>
#include <cmath>

namespace rc {

uint32_t fnv1a32Lower(std::string_view name) {
  uint32_t h = 2166136261u;
  for (char c : name) {
    unsigned char b = static_cast<unsigned char>(c);
    if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
    h ^= b;
    h *= 16777619u;
  }
  return h;
}

uint16_t nickHue(std::string_view name) { return static_cast<uint16_t>(fnv1a32Lower(name) % 360u); }

uint16_t hslToRgb565(float hDeg, float s, float l) {
  const float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
  const float hp = std::fmod(hDeg, 360.0f) / 60.0f;
  const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
  float r = 0, g = 0, b = 0;
  if (hp < 1) r = c, g = x;
  else if (hp < 2) r = x, g = c;
  else if (hp < 3) g = c, b = x;
  else if (hp < 4) g = x, b = c;
  else if (hp < 5) r = x, b = c;
  else r = c, b = x;
  const float m = l - c / 2.0f;
  auto to8 = [m](float v) { return static_cast<uint32_t>(std::lround((v + m) * 255.0f)); };
  const uint32_t R = to8(r), G = to8(g), B = to8(b);
  return static_cast<uint16_t>(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
}

uint16_t nickColor(const Theme& t, std::string_view name, bool isSelf) {
  if (isSelf) return t.selfNick;
  if (!t.colorNicks) return t.nick;
  return hslToRgb565(nickHue(name), 0.70f, t.nickLightness / 100.0f);
}


bool sameUsername(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}
}  // namespace rc
