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

// The boot splash: the text-art monster mascot (`art`; nullptr, empty or all
// blank lines mean Fluffy) over the name, `detail` (version and board) and a
// "> booting" prompt, centred in a cols x rows grid. The art is left out when
// it doesn't fit; text is clipped to the width. A non-empty `say` puts a
// speech bubble beside the monster, its tail pointing at him, wrapped to fit;
// it is left out when there isn't room. maxScale > 1 lets the art grow, up to
// that many times, on a grid with room for it (the X4's tall panel); the
// bubble then sits above it, its tail pointing down.
std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail,
                                     std::string_view say = {}, int maxScale = 1,
                                     const std::vector<std::string>* art = nullptr);

// Hands out `phrases` (empty: defaultMascotPhrases()) in random order: each
// once per round, and never the same one twice running. A list different from
// the last call's starts a new round. `rnd` is any random number.
class MonsterWords {
 public:
  std::string next(uint32_t rnd, const std::vector<std::string>& phrases);

 private:
  std::vector<std::string> list_;
  std::vector<int> order_;
  size_t pos_ = 0;
  int last_ = -1;
};

}  // namespace rc
