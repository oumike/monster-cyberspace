#pragma once

#include <string>

#include "guild/guild.h"
#include "input/key_event.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/qr_box.h"
#include "lvui/views/text_box.h"
#include "screens/link_view.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/text_view.h"

namespace rc {

// A guild's Info page (spec §5): scrolling text; Centre on a guild with a
// link shows it full screen with its QR code.
class GuildInfoView {
 public:
  void open(const Guild& g);
  void close() { open_ = linkOpen_ = false; }
  bool isOpen() const { return open_; }
  // False once the view has closed.
  bool key(const KeyEvent& k);
  // The text into text (over rows 1 to the last), or the
  // link into cells (between the caller's begin() and end()) and qr; then
  // the hint.
  void sync(lvui::TextBox& text, lvui::CellText& cells, lvui::QrBox& qr, lvui::HintBar& hint);

 private:
  bool open_ = false, linkOpen_ = false;
  std::string link_;
  TextView text_;
  LinkView linkView_;
};

}  // namespace rc
