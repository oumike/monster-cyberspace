#include "features.h"
#if RC_FEAT_GUILDS

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/people_nav.h"
#include "screens/screens.h"
#include "store/locked_feed.h"
#include "ui/guild_layout.h"

namespace rc {

void MembersScreen::enter(const std::string& slug) {
  slug_ = slug;
  asked_ = failed_ = false;
  list_.setItems({});
  list_.setSelected(0);
  Command c{CmdType::OpenMembers};
  c.a = slug_;
  feedSource().command(c);
}

void MembersScreen::requestMore() {
  asked_ = true;
  Command c{CmdType::OpenMembers};
  c.a = slug_;
  c.n = 1;
  feedSource().command(c);
}

void MembersScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      // The last row pages in more before wrapping.
      if (list_.selected() == int(list_.size()) - 1 && more_) {
        if (!asked_) requestMore();
      } else {
        list_.down();
      }
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) {
        if (list_.selected() == int(list_.size()) - 1 && more_ && !asked_) requestMore();
        onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right: {
      int i = list_.selected();
      if (i >= 0 && i < int(names_.size()) && !names_[i].empty()) people::openProfile(names_[i]);
      break;
    }
    case Action::Left:
    case Action::Back:
      router::go(router::Id::Guild, slug_);
      break;
    default:
      break;
  }
}

void MembersScreen::onEvent(const Event& e) {
  if (e.type != EvtType::MembersUpdated) return;
  asked_ = false;
  failed_ = !e.ok;
}

void MembersScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  std::vector<Member> members;
  std::u32string name;
  lockedFeed().with([&](FeedState& s) {
    more_ = !s.guilds.membersCursor().empty();
    members = s.guilds.members();
    const Guild* gd = s.guilds.guild();
    if (gd) name = utf8ToU32(gd->name.empty() ? gd->slug : gd->name);
  });
  std::vector<ListItem> items;
  items.reserve(members.size());
  names_.clear();
  for (const auto& m : members) {
    items.push_back(memberItem(m, nowMs));
    names_.push_back(m.username);
  }
  bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });
  const int nav = sideNavWidth(g);
  const int bottom = g.rows() - 1;
  g.fill(t.bg);
  drawStatus(g, t, U"members \u00b7 " + name, -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  list_.render(g, 1, bottom, 0, g.cols() - nav, t);
  if (empty) {
    const char* msg = loading && app().conn == 0 ? "offline"
                      : failed_                  ? "couldn't load"
                      : loading                  ? "loading\u2026"
                                                 : "no members";
    putLine(g, 1, 2, msg, t.dim, t.bg);
  }
  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (asked_) drawHint(g, t, U"loading more\u2026", U"loading more\u2026");
  else drawHint(g, t, U"\u2191\u2193 pick  \u25CF profile  \u2190 guild",
                U"tap a member  \u2039 guild");
}

}  // namespace rc

#endif  // RC_FEAT_GUILDS
