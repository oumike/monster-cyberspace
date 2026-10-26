#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rc {

enum class SplashRole { Art, Title, Detail, Prompt };

struct SplashLine {
  int col, row;
  std::string text;  // UTF-8
  SplashRole role;
};

// The boot splash: a text-art platypus over the name, `detail` (version and
// board) and a "> booting" prompt, centred in a cols x rows grid. The art is
// left out when it doesn't fit; text is clipped to the width.
std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail);

}  // namespace rc
