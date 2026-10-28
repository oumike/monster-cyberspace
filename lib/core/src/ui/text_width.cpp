#include "ui/text_width.h"

#include "ui/emoji_set.h"

namespace rc {

namespace {
constexpr int kEmojiCount = static_cast<int>(sizeof(kTrayEmoji) / sizeof(kTrayEmoji[0]));
}

int emojiIndex(char32_t cp) {
  if (cp < 0x2700) return -1;
  for (int i = 0; i < kEmojiCount; ++i)
    if (kTrayEmoji[i] == cp) return i;
  return -1;
}

char32_t wideTail(char32_t cp) {
  const int i = emojiIndex(cp);
  return i < 0 ? 0 : kWideTailBase + static_cast<char32_t>(i);
}

bool isWideTail(char32_t cp) { return cp >= kWideTailBase && cp < kWideTailBase + kEmojiCount; }

int cellWidth(char32_t cp) {
  if (cp == 0x200D || cp == 0xFE0E || cp == 0xFE0F) return 0;
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
