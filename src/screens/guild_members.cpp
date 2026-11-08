#include "feature_flags.h"
#if RC_FEAT_GUILDS

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/lv_draw.h"
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

std::vector<ListItem> MembersScreen::rows(int64_t nowMs, std::u32string& name) {
  std::vector<Member> members;
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
  return items;
}

const char* MembersScreen::emptyText(bool loading) const {
  return loading && app().conn == 0 ? "offline"
         : failed_                  ? "couldn't load"
         : loading                  ? "loading\u2026"
                                    : "no members";
}

void MembersScreen::hints(const char32_t*& keys, const char32_t*& touch) const {
  if (asked_) {
    keys = touch = U"loading more\u2026";
  } else {
    keys = U"\u2191\u2193 pick  \u25CF profile  \u2190 guild";
    touch = U"tap a member  \u2039 guild";
  }
}

void MembersScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  std::u32string name;
  std::vector<ListItem> items = rows(nowMs, name);
  bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });
  const int nav = sideNavWidth(g);
  const int bottom = g.rows() - 1;
  g.fill(t.bg);
  drawStatus(g, t, U"members \u00b7 " + name, -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  list_.render(g, 1, bottom, 0, g.cols() - nav, t);
  if (empty) putLine(g, 1, 2, emptyText(loading), t.dim, t.bg);
  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  const char32_t* keys;
  const char32_t* touch;
  hints(keys, touch);
  drawHint(g, t, keys, touch);
}

// ---- LVGL ----

void MembersScreen::build(lv_obj_t* root) {
  listView_.create(root, /*windowed*/ true);
  navView_.create(root);
  text_.create(root);
  hint_.create(root);
  status_.create(root);
  sync(nowMs_);
}

void MembersScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  // The members and the guild's name change with MembersUpdated and
  // GuildUpdated, which ui::inputSeq() counts; the store's loading flag (set
  // with no event) and the ages show with the once-a-second refresh.
  if (gate_.due(text_.obj(), nowMs, {int64_t(app().conn)})) sync(nowMs);
}

void MembersScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  std::u32string name;
  std::vector<ListItem> items = rows(nowMs, name);
  const bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });
  const int cols = text_.cols(), bottom = text_.rows() - 1;
  const int nav = sideNavWidth(text_);
  syncStatus(status_, U"members \u00b7 " + name, -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  placeCells(listView_.obj(), 0, 1, cols - nav, bottom);
  listView_.sync(list_);
  text_.begin();
  if (empty) text_.line(1, 2, emptyText(loading), t.dim);
  text_.end();
  navView_.sync(nav, 1, bottom, cols);
  const char32_t* keys;
  const char32_t* touch;
  hints(keys, touch);
  syncHint(hint_, keys, touch);
}

}  // namespace rc

#endif  // RC_FEAT_GUILDS
