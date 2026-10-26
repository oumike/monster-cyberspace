#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"

namespace rc {

void RoomListScreen::enter(const std::string&) {
  chatSource().command(Command{CmdType::RefreshRooms});
}

void RoomListScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      list_.down();
      break;
    case Action::Select:
    case Action::Right:
      if (list_.selected() < int(slugs_.size())) {
        app().cfg.lastRoom = slugs_[list_.selected()];
        settings::saveUiPrefs(app().cfg);
        router::go(router::Id::Room, app().cfg.lastRoom);
      }
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
      auto it = s.readAt.find(r.slug);
      bool unread = it == s.readAt.end() ? r.lastMessageAt > 0 : r.lastMessageAt > it->second;
      items.push_back(ListItem{utf8ToU32("#" + r.slug), utf8ToU32(std::to_string(r.onlineCount)),
                               ItemTone::Normal, unread});
      slugs_.push_back(r.slug);
    }
  });
  g.fill(t.bg);
  drawStatus(g, t, U"rooms", int(items.size()));
  if (items.empty()) putLine(g, 1, 2, "loading rooms\u2026", t.dim, t.bg);
  list_.setItems(std::move(items));
  list_.render(g, 1, g.rows() - 1, 0, g.cols(), t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home");
}

}  // namespace rc
