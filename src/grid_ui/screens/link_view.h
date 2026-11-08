#pragma once

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// A full-screen link view below the status bar: the URL as text, then a QR
// code of it when one fits. Shared by the room and the entry screens.
class LinkView {
 public:
  void open(const std::string& url);
  // Draws rows 1 to the last; the last row holds the hint.
  void render(Grid& g, const Theme& t) const;

 private:
  std::u32string url_;
  std::vector<std::u32string> qr_;
};

}  // namespace rc
