#pragma once

#include <string>
#include <vector>

#include "lvui/views/cell_text.h"
#include "lvui/views/qr_box.h"
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
  // render for LVGL: the lines into text (between the caller's begin() and
  // end()), the QR code into qr, and the hint.
  void sync(lvui::CellText& text, lvui::QrBox& qr, lvui::HintBar& hint) const;

 private:
  std::u32string url_;
  std::string urlUtf8_;
  std::vector<std::u32string> qr_;
};

}  // namespace rc
