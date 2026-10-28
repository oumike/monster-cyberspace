#include "ui/text_width.h"

#include <algorithm>
#include <iterator>

#include "ui/emoji_set.h"
#include "ui/emoji_wide.h"

namespace rc {

namespace {
constexpr int kEmojiCount = static_cast<int>(std::size(kWideEmoji));
}

int emojiIndex(char32_t cp) {
  if (cp < kWideEmoji[0]) return -1;
  const char32_t* e = std::lower_bound(std::begin(kWideEmoji), std::end(kWideEmoji), cp);
  return e != std::end(kWideEmoji) && *e == cp ? static_cast<int>(e - std::begin(kWideEmoji)) : -1;
}

char32_t wideTail(char32_t cp) {
  const int i = emojiIndex(cp);
  return i < 0 ? 0 : kWideTailBase + static_cast<char32_t>(i);
}

bool isWideTail(char32_t cp) { return cp >= kWideTailBase && cp < kWideTailBase + kEmojiCount; }

int cellWidth(char32_t cp) {
  // ZWJ, variation selectors, the keycap mark, skin tones and tags.
  if (cp == 0x200D || cp == 0xFE0E || cp == 0xFE0F || cp == 0x20E3) return 0;
  if ((cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0000 && cp <= 0xE007F)) return 0;
  return emojiIndex(cp) >= 0 ? 2 : 1;
}

int textWidth(std::u32string_view s) {
  int n = 0;
  for (char32_t cp : s) n += cellWidth(cp);
  return n;
}

size_t fitChars(std::u32string_view s, int cols) {
  int used = 0;
  size_t i = 0;
  for (; i < s.size(); ++i) {
    const int w = cellWidth(s[i]);
    if (used + w > cols) break;
    used += w;
  }
  return i;
}

}  // namespace rc
