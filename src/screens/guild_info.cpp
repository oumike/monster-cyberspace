#include "screens/guild_info.h"

#include "screens/draw.h"
#include "ui/guild_layout.h"

namespace rc {

void GuildInfoView::open(const Guild& g) {
  open_ = true;
  linkOpen_ = false;
  link_ = g.link;
  text_.setText(guildInfoText(g));
}

bool GuildInfoView::key(const KeyEvent& k) {
  if (!open_) return false;
  if (k.action == Action::Activate) return key(KeyEvent{k.then});
  if (linkOpen_) {
    if (k.action == Action::Back || k.action == Action::Select || k.action == Action::Left)
      linkOpen_ = false;
    return true;
  }
  switch (k.action) {
    case Action::Up:
      text_.up();
      break;
    case Action::Down:
      text_.down();
      break;
    case Action::Select:
      if (!link_.empty()) {
        linkView_.open(link_);
        linkOpen_ = true;
      }
      break;
    case Action::Back:
    case Action::Left:
      open_ = false;
      break;
    default:
      break;
  }
  return open_;
}

void GuildInfoView::render(Grid& g, const Theme& t) {
  if (linkOpen_) return linkView_.render(g, t);
  const int bottom = g.rows() - 1;
  text_.render(g, 1, bottom, 0, g.cols(), t);
  drawHint(g, t, link_.empty() ? U"\u2191\u2193 scroll  back: close" : U"\u2191\u2193 scroll  \u25CF link  back: close",
           link_.empty() ? U"swipe to scroll  \u2039 close" : U"swipe to scroll  hold: link  \u2039 close");
}

}  // namespace rc
