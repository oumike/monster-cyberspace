#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// The website's nick colour: hue = FNV-1a (32-bit) of the lower-cased
// username, mod 360. Lightness is ours, chosen for a dark background.
uint32_t fnv1a32Lower(std::string_view name);
uint16_t nickHue(std::string_view name);
uint16_t hslToRgb565(float hDeg, float s, float l);
// Your own nick is always gold (#FFD166).
uint16_t nickColor(std::string_view name, bool isSelf);

}  // namespace rc
