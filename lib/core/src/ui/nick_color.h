#pragma once

#include <cstdint>
#include <string_view>

#include "ui/theme.h"

namespace rc {

// The website's nick colour: hue = FNV-1a (32-bit) of the lower-cased
// username, mod 360. Lightness is ours, chosen for a dark background.
uint32_t fnv1a32Lower(std::string_view name);
uint16_t nickHue(std::string_view name);
uint16_t hslToRgb565(float hDeg, float s, float l);
// With a colour theme, your own nick is gold (#FFD166) and others take their
// hue; a monochrome theme gives its own nick colours.
uint16_t nickColor(const Theme& t, std::string_view name, bool isSelf);
// Usernames match without regard to ASCII case.
bool sameUsername(std::string_view a, std::string_view b);

}  // namespace rc
