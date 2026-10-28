#pragma once

#include <cstddef>
#include <string_view>

namespace rc {

// Cell widths: emoji (kWideEmoji) take two cells; variation selectors, the
// zero width joiner, the keycap mark, skin tone modifiers and tags none;
// everything else one.

// The index of cp in kWideEmoji, or -1.
int emojiIndex(char32_t cp);
// The code point that draws the right half of wide cp (kWideTailBase + index).
char32_t wideTail(char32_t cp);
bool isWideTail(char32_t cp);
int cellWidth(char32_t cp);
int textWidth(std::u32string_view s);
// How many code points from the start of s fit in `cols` cells.
size_t fitChars(std::u32string_view s, int cols);

}  // namespace rc
