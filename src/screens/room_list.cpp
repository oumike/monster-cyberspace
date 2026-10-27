#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "ui/button_grid.h"
#include "ui/tap_map.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"

namespace rc {

void RoomListScreen::enter(const std::string&) {
  chatSource().command(Command{CmdType::RefreshRooms});
}

void RoomListScreen::open(int index) {
  if (index < 0 || index >= int(slugs_.size())) return;
  app().cfg.lastRoom = slugs_[index];
  settings::saveUiPrefs(app().cfg);
  router::go(router::Id::Room, app().cfg.lastRoom);
}

void RoomListScreen::onKey(const KeyEvent& k) {
  constexpr int kPitch = kButtonRows + 1;
  switch (k.action) {
    case Action::Up:
      if (!buttons_) {
        list_.up();
      } else if (--(dragSteps_ = std::min(dragSteps_, 0)) <= -kPitch) {
        dragSteps_ = 0;
        --scroll_;  // clamped in render
      }
      break;
    case Action::Down:
      if (!buttons_) {
        list_.down();
      } else if (++(dragSteps_ = std::max(dragSteps_, 0)) >= kPitch) {
        dragSteps_ = 0;
        ++scroll_;
      }
      break;
    case Action::Activate:
      if (k.target != uint8_t(ListId::Main)) break;
      if (buttons_) {
        if (k.then == Action::Select) open(k.index);
      } else if (list_.select(k.index)) {
        onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right:
      if (!buttons_) open(list_.selected());
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
  buttons_ = board::hasTouch() && !board::hasKeys();
  if (buttons_) return renderButtons(g, t, items);
  list_.setItems(std::move(items));
  list_.render(g, 1, g.rows() - 1, 0, g.cols(), t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
}

void RoomListScreen::renderButtons(Grid& g, const Theme& t, const std::vector<ListItem>& items) {
  const int n = int(items.size());
  const ButtonGrid b = buttonGrid(g.cols(), 2, g.rows() - 1, n, 3, scroll_);
  scroll_ = b.scrollRow;
  if (activeTaps()) activeTaps()->setList(ListId::Main, -1, n, false);
  for (size_t k = 0; k < b.rects.size(); ++k) {
    const int i = b.first + int(k);
    const TileRect& r = b.rects[k];
    const ListItem& it = items[i];
    const int mid = r.r0 + kButtonRows / 2;
    const int inW = r.c1 - r.c0 - 1;  // inside the outline
    // The online count at the right when the name leaves room for it.
    const int cw = int(it.right.size());
    const bool count = cw > 0 && int(it.label.size()) + 1 + cw <= inW;
    if (count) g.putText(r.c1 - cw, mid, it.right, t.dim, t.bg, cw);
    g.putText(r.c0 + 1, mid, it.label, it.marker ? t.selectBg : t.fg, t.bg,
              count ? inW - cw - 1 : inW);
    g.outline(r.c0, r.r0, r.c1, r.r1, t.dim);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Main;
      tt.index = static_cast<int16_t>(i);
      activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt);
    }
  }
  drawHint(g, t, U"", U"tap to open  drag to scroll");
}

}  // namespace rc
