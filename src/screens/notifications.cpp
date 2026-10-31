#include "app/app_state.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_feed.h"

namespace rc {

void NotificationsScreen::open(int index) {
  if (index < 0 || index >= int(targets_.size())) return;
  const auto& t = targets_[index];
  if (t.first) {
    app().cfg.lastRoom = t.second;
    settings::saveUiPrefs(app().cfg);
    router::go(router::Id::Room, t.second);
  } else {
    router::go(router::Id::Entry, t.second);
  }
}

void NotificationsScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      list_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right:
      open(list_.selected());
      break;
    case Action::Left:
    case Action::Back:
      router::go(router::Id::Home);
      break;
    default:
      break;
  }
}

void NotificationsScreen::render(Grid& g, const Theme& t, int64_t) {
  std::vector<ListItem> items;
  targets_.clear();
  const auto& ms = app().mentions;
  for (auto it = ms.rbegin(); it != ms.rend(); ++it) {
    items.push_back(ListItem{utf8ToU32(it->text), U"", ItemTone::Normal, true});
    targets_.emplace_back(true, it->slug);
  }
  lockedFeed().with([&](FeedState& s) {
    for (auto it = s.notices.rbegin(); it != s.notices.rend(); ++it) {
      items.push_back(ListItem{utf8ToU32(it->text), U"", ItemTone::Normal, true});
      targets_.emplace_back(false, it->postId + '\n' + it->id);
    }
  });
  g.fill(t.bg);
  drawStatus(g, t, U"notifications", int(items.size()));
  if (items.empty()) putLine(g, 1, 2, "nothing unread", t.dim, t.bg);
  list_.setItems(std::move(items));
  list_.render(g, 1, g.rows() - 1, 0, g.cols(), t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
}

}  // namespace rc
