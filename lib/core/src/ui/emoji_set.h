#pragma once

// The emoji the tray offers, by kind: camillia-mt's kEmojiTray, in its order,
// with more faces, hands and a note among them. All emoji the fonts draw are
// in emoji_wide.h. scripts/emoji2cells.py parses this file: keep one hex
// literal per entry.

namespace rc {

constexpr char32_t kTrayEmoji[] = {
    // Faces
    0x1F600, 0x1F602, 0x1F603, 0x1F604, 0x1F609, 0x1F60A, 0x1F60D, 0x1F618,
    0x1F60E, 0x1F914, 0x1F610, 0x1F644, 0x1F60F, 0x1F622, 0x1F62D, 0x1F621,
    0x1F631, 0x1F633, 0x1F634, 0x1F925, 0x1F92F, 0x1F642, 0x1F643, 0x1F615,
    0x1F62C, 0x1F605, 0x1F606, 0x1F923, 0x1F607, 0x1F970, 0x1F61C, 0x1F92A,
    0x1F917, 0x1F92D, 0x1F971, 0x1F97A, 0x1F92C, 0x1F608, 0x1F480, 0x1F47B,
    0x1F916, 0x1F4A9, 0x1F973, 0x1F913, 0x1F92E,
    // Hands
    0x1F44D, 0x1F44E, 0x1F44C, 0x1F44B, 0x1F44F, 0x1F64F, 0x1F4AA, 0x1F91D,
    0x0270C, 0x1F44A, 0x1F595, 0x1F64C, 0x1F91E, 0x1F918,
    // Symbols
    0x02764, 0x1F494, 0x1F525, 0x02B50, 0x02705, 0x0274C, 0x02757, 0x02753,
    0x1F4A1, 0x1F4AF, 0x1F440, 0x1F4CD,
    // Celebrate and objects
    0x1F389, 0x1F38A, 0x1F381, 0x1F680, 0x1F3B5,
    // Weather and nature
    0x02600, 0x02601, 0x1F327, 0x026A1, 0x02744, 0x1F30A,
    // Food and drink
    0x1F355, 0x02615, 0x1F37A, 0x1F36A, 0x1F34E,
};

// The right half of kWideEmoji[i] is drawn from code point kWideTailBase + i,
// in a private use plane.
constexpr char32_t kWideTailBase = 0xF0000;

// The tray draws its emoji large where they fit: kTrayEmoji[i] over
// kTrayBigCols x kTrayBigRows cells, the cell at (col c, row r) drawn from
// code point kTrayBigBase + i * kTrayBigCols * kTrayBigRows + r * kTrayBigCols + c.
constexpr int kTrayBigCols = 4, kTrayBigRows = 2;
constexpr char32_t kTrayBigBase = 0xF8000;
constexpr int kTrayCount = sizeof(kTrayEmoji) / sizeof(kTrayEmoji[0]);

constexpr char32_t trayBigPiece(int i, int r, int c) {
  return kTrayBigBase + static_cast<char32_t>((i * kTrayBigRows + r) * kTrayBigCols + c);
}
constexpr bool isTrayBigPiece(char32_t cp) {
  return cp >= kTrayBigBase &&
         cp < kTrayBigBase + static_cast<char32_t>(kTrayCount * kTrayBigCols * kTrayBigRows);
}

}  // namespace rc
