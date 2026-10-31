#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

enum class SplashRole { Art, Title, Detail, Prompt, Bubble };

struct SplashLine {
  int col, row;
  std::string text;  // UTF-8
  SplashRole role;
  // Art drawn this many times bigger (Grid::setRowScale); col then counts
  // cells of the scaled width.
  int scale = 1;
};

// The boot splash: Fluffy, the text-art monster mascot, over the name,
// `detail` (version and board) and a "> booting" prompt, centred in a
// cols x rows grid. The art is left out when it doesn't fit; text is clipped
// to the width. A non-empty `say` puts a speech bubble beside Fluffy, its
// tail pointing at him, wrapped to fit; it is left out when there isn't room.
// maxScale > 1 lets the art grow, up to that many times, on a grid with room
// for it (the X4's tall panel); the bubble then sits above it, its tail
// pointing down.
std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail,
                                     std::string_view say = {}, int maxScale = 1);

// What Fluffy says in his bubble.
constexpr int kMonsterWordCount = 6;
extern const char* const kMonsterWords[kMonsterWordCount];

// Hands out the words in random order: each once per round, and never the
// same word twice running. `rnd` is any random number.
class MonsterWords {
 public:
  const char* next(uint32_t rnd);

 private:
  int order_[kMonsterWordCount] = {};
  int pos_ = kMonsterWordCount;
  int last_ = -1;
};

}  // namespace rc
