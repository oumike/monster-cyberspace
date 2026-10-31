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
};

// The boot splash: a text-art monster over the name, `detail` (version and
// board) and a "> booting" prompt, centred in a cols x rows grid. The art is
// left out when it doesn't fit; text is clipped to the width. A non-empty
// `say` puts a speech bubble beside the monster, its tail pointing at it,
// wrapped to fit; it is left out when there isn't room.
std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail,
                                     std::string_view say = {});

// What the monster says in its bubble.
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
