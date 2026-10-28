#pragma once

// The emoji the tray offers and the cell fonts draw, two cells wide: the first
// 40 of camillia-mt's kEmojiTray, in its order. scripts/emoji2cells.py parses
// this file: keep one hex literal per entry.

namespace rc {

constexpr char32_t kTrayEmoji[] = {
    0x1F600, 0x1F602, 0x1F603, 0x1F604, 0x1F609, 0x1F60A, 0x1F60D, 0x1F618,
    0x1F60E, 0x1F914, 0x1F610, 0x1F644, 0x1F60F, 0x1F622, 0x1F62D, 0x1F621,
    0x1F631, 0x1F633, 0x1F634, 0x1F925, 0x1F92F, 0x1F642, 0x1F643, 0x1F615,
    0x1F62C, 0x1F44D, 0x1F44E, 0x1F44C, 0x1F44B, 0x1F44F, 0x1F64F, 0x1F4AA,
    0x1F91D, 0x0270C, 0x1F44A, 0x1F595, 0x02764, 0x1F494, 0x1F525, 0x02B50,
};

// The right half of kTrayEmoji[i] is drawn from code point kWideTailBase + i,
// in a private use plane.
constexpr char32_t kWideTailBase = 0xF0000;

}  // namespace rc
