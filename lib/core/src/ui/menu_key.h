#pragma once

#include <string>
#include <string_view>

namespace rc {

// Hints name the commands key "ctrl" (the M9's). A board whose commands key
// is another one (board::menuKeyName) gets that name instead: each whole word
// "ctrl" or "Ctrl" becomes `key`.
inline std::u32string withMenuKey(std::u32string_view text, std::u32string_view key) {
  std::u32string out(text);
  if (key == U"ctrl") return out;
  auto letter = [](char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); };
  for (size_t i = 0; i + 4 <= out.size();) {
    const std::u32string_view w(out.data() + i, 4);
    const bool word = (w == U"ctrl" || w == U"Ctrl") && (i == 0 || !letter(out[i - 1])) &&
                      (i + 4 == out.size() || !letter(out[i + 4]));
    if (!word) {
      ++i;
      continue;
    }
    out.replace(i, 4, key);
    i += key.size();
  }
  return out;
}

}  // namespace rc
