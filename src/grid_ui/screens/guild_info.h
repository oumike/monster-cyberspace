#pragma once

#include <string>

#include "guild/guild.h"
#include "input/key_event.h"
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
  // Draws rows 1 to the last (the last holds the hint).
  void render(Grid& g, const Theme& t);

 private:
  bool open_ = false, linkOpen_ = false;
  std::string link_;
  TextView text_;
  LinkView linkView_;
};

}  // namespace rc
