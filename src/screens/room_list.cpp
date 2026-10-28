#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "screens/button_pad.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"

namespace rc {

void RoomListScreen::enter(const std::string&) {
  filter_.clear();
  chatSource().command(Command{CmdType::RefreshRooms});
}

void RoomListScreen::open(int index) {
  if (index < 0 || index >= int(slugs_.size())) return;
  app().cfg.lastRoom = slugs_[index];
  settings::saveUiPrefs(app().cfg);
  router::go(router::Id::Room, app().cfg.lastRoom);
}

void RoomListScreen::onKey(const KeyEvent& k) {
  if (buttons_ && k.action != Action::Left && k.action != Action::Back) return open(pad_.key(k));
  if (filter_.key(k)) {
    list_.setSelected(0);
    return;
  }
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

void RoomListScreen::render(Grid& g, const Theme& t, int64_t) {
  std::vector<ListItem> items;
  slugs_.clear();
  lockedChat().with([&](ChatState& s) {
    std::vector<RoomInfo> rooms = s.rooms;
    std::stable_sort(rooms.begin(), rooms.end(), [](const RoomInfo& a, const RoomInfo& b) {
      if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
      return a.lastMessageAt > b.lastMessageAt;
    });
    for (const auto& r : rooms) {
      if (!filter_.matches(utf8ToU32(r.slug))) continue;
      auto it = s.readAt.find(r.slug);
      bool unread = it == s.readAt.end() ? r.lastMessageAt > 0 : r.lastMessageAt > it->second;
      items.push_back(ListItem{utf8ToU32("#" + r.slug), utf8ToU32(std::to_string(r.onlineCount)),
                               ItemTone::Normal, unread});
      slugs_.push_back(r.slug);
    }
  });
  g.fill(t.bg);
  drawStatus(g, t, U"rooms", int(items.size()));
  if (items.empty())
    putLine(g, 1, 2, filter_.active() ? "no rooms match" : "loading rooms\u2026", t.dim, t.bg);
  buttons_ = ButtonPad::active();
  if (buttons_) {
    pad_.render(g, t, items, 2, g.rows() - 1, false);
    return drawHint(g, t, U"", U"tap to open  drag to scroll");
  }
  list_.setItems(std::move(items));
  list_.render(g, 1, g.rows() - 1, 0, g.cols(), t);
  if (filter_.active())
    return drawHint(g, t, U"filter: " + filter_.text() + U"\u2581  \u232B erase  back clears");
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
}

}  // namespace rc
