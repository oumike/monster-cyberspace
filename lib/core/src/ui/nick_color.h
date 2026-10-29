#pragma once

#include <cstdint>
#include <string_view>

#include "ui/theme.h"

namespace rc {

// The website's nick colour: hue = FNV-1a (32-bit) of the lower-cased
// username, mod 360. Lightness is the theme's (Theme::nickLightness).
uint32_t fnv1a32Lower(std::string_view name);
uint16_t nickHue(std::string_view name);
uint16_t hslToRgb565(float hDeg, float s, float l);
// Your own nick is the theme's selfNick (Default: gold #FFD166). With
// colorNicks others take their hue; otherwise the theme's nick colour.
uint16_t nickColor(const Theme& t, std::string_view name, bool isSelf);
// Usernames match without regard to ASCII case.
bool sameUsername(std::string_view a, std::string_view b);

}  // namespace rc
