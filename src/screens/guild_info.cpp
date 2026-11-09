#include "feature_flags.h"
#if RC_FEAT_ENTRIES

#include "screens/guild_info.h"

#include "screens/lv_draw.h"
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

void GuildInfoView::sync(lvui::TextBox& text, lvui::CellText& cells, lvui::QrBox& qr, lvui::HintBar& hint) {
  lvui::setHidden(text.obj(), linkOpen_);
  if (linkOpen_) return linkView_.sync(cells, qr, hint);
  lvui::setHidden(qr.obj(), true);
  placeCells(text.obj(), 0, 1, cells.cols(), cells.rows() - 1);
  text.sync(text_);
  syncHint(hint, link_.empty() ? U"\u2191\u2193 scroll  back: close" : U"\u2191\u2193 scroll  \u25CF link  back: close",
           link_.empty() ? U"swipe to scroll  \u2039 close" : U"swipe to scroll  hold: link  \u2039 close");
}

}  // namespace rc

#endif  // RC_FEAT_ENTRIES
