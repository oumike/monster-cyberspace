#pragma once

#include <cstdint>
#include <string_view>

namespace rc {

// The keyboard backlight: four steps, after camillia-mt's (Low 64, Medium 160,
// High 255 of 255). The values are saved: never reorder.
enum class KbLight : uint8_t { Off = 0, Low = 1, Medium = 2, High = 3 };
constexpr int kKbLightCount = 4;

uint8_t kbLightDuty(KbLight l);       // 0..255
const char* kbLightKey(KbLight l);    // "off", "low", "medium", "high"
const char32_t* kbLightLabel(KbLight l);  // U"Off" ...
// Out of range gives fallback.
KbLight kbLightFromIndex(int i, KbLight fallback = KbLight::Off);
bool parseKbLight(std::string_view key, KbLight& out);  // out unchanged on failure

}  // namespace rc
