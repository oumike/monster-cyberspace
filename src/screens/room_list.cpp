#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/room_read.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "screens/button_pad.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"

namespace rc {

void RoomListScreen::enter(const std::string&) {
  filter_.clear();
  pad_.reset();
  closeOverlays();
  marking_ = false;
  chatSource().command(Command{CmdType::RefreshRooms});
}

void RoomListScreen::askMarkAll() {
  if (marking_) return;
  confirm_ = true;
  box_.confirm(U"Mark all " + utf8ToU32(std::to_string(unread_)) +
                   U" rooms read? (also on the website)",
               U"Mark all read");
}

void RoomListScreen::open(int index) {
  if (index < 0 || index >= int(slugs_.size())) return;
  if (slugs_[index].empty()) return askMarkAll();
  app().cfg.lastRoom = slugs_[index];
  settings::saveUiPrefs(app().cfg);
  router::go(router::Id::Room, app().cfg.lastRoom);
}

void RoomListScreen::onKey(const KeyEvent& k) {
  if (confirm_) {
    switch (box_.key(k)) {
      case Overlay::Result::Picked:
        confirm_ = false;
        if (box_.selected() != 1) break;
        if (app().conn == 0) {
          ui::banner("Offline: not sent", true);
          break;
        }
        marking_ = true;
        chatSource().command(Command{CmdType::MarkRoomsRead});
        break;
      case Overlay::Result::Closed:
        confirm_ = false;
        break;
      case Overlay::Result::None:
        break;
    }
    return;
  }
  if (filter_.key(k)) {
    list_.setSelected(0);
    pad_.reset();
    return;
  }
  // With keys, Left moves through the buttons until the first column.
  const bool leave = k.action == Action::Back ||
                     (k.action == Action::Left && (!board::hasKeys() || pad_.atLeftEdge()));
  if (buttons_ && !leave) return open(pad_.key(k));
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

void RoomListScreen::onEvent(const Event& e) {
  if (e.type == EvtType::RoomsRead) marking_ = false;
}

std::vector<ListItem> RoomListScreen::rows(int& shown) {
  std::vector<ListItem> items;
  slugs_.clear();
  shown = 0;
  lockedChat().with([&](ChatState& s) {
    unread_ = int(unreadRooms(s.rooms, s.readAt).size());
    // Pinned first while anything is unread, as on the Unread screen.
    if ((unread_ > 0 || marking_) && !filter_.active()) {
      items.push_back(ListItem{marking_ ? U"marking all read\u2026" : U"\u2713 Mark all read", U""});
      slugs_.push_back("");
    }
    std::vector<RoomInfo> rooms = s.rooms;
    std::stable_sort(rooms.begin(), rooms.end(), [](const RoomInfo& a, const RoomInfo& b) {
      if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
      return a.lastMessageAt > b.lastMessageAt;
    });
    for (const auto& r : rooms) {
      if (!filter_.matches(utf8ToU32(r.slug))) continue;
      const bool unread = roomUnread(r, s.readAt);
      // ListView draws the marker as a dot; buttons only tint it, so they
      // get the dot in the label.
      std::u32string label = utf8ToU32("#" + r.slug);
      if (unread && board::hasTouch()) label = U"\u2022 " + label;
      items.push_back(ListItem{std::move(label), utf8ToU32(std::to_string(r.onlineCount)),
                               ItemTone::Normal, unread});
      slugs_.push_back(r.slug);
      ++shown;
    }
  });
  return items;
}

void RoomListScreen::build(lv_obj_t* root) {
  text_.create(root);
  listView_.create(root);
  padView_.create(root);
  hint_.create(root);
  status_.create(root);
  menuView_.create(root);
  sync();
}

void RoomListScreen::update(int64_t nowMs) {
  // The rooms and their read marks change with an event (RoomsUpdated,
  // RoomsRead), which ui::inputSeq() counts.
  if (gate_.due(text_.obj(), nowMs)) sync();
}

void RoomListScreen::sync() {
  const Theme& t = lvui::currentTheme();
  int shown = 0;
  std::vector<ListItem> items = rows(shown);
  const int bottom = text_.rows() - 1;
  syncStatus(status_, U"rooms", shown);
  text_.begin();
  if (shown == 0) text_.line(1, 2, filter_.active() ? "no rooms match" : "loading rooms\u2026", t.dim);
  text_.end();
  buttons_ = board::hasTouch();
  pad_.setCursor(board::hasKeys());
  lvui::setHidden(listView_.obj(), buttons_);
  lvui::setHidden(padView_.obj(), !buttons_);
  if (buttons_) {
    pad_.sync(padView_, items, 2, bottom, false);
  } else {
    list_.setItems(std::move(items));
    placeCells(listView_.obj(), 0, 1, text_.cols(), bottom);
    listView_.sync(list_);
  }
  const lvui::Cell c = text_.cell();
  menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, 0, c.h);
  menuView_.sync(box_);
  if (confirm_)
    return syncHint(hint_, U"\u2191\u2193 pick  \u25CF choose  back: cancel", U"tap: choose  outside: cancel");
  if (filter_.active()) return syncHint(hint_, U"filter: " + filter_.text() + U"\u2581  \u232B erase  back clears");
  syncHint(hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home",
           buttons_ ? U"tap to open  drag to scroll" : U"tap to open  swipe to scroll");
}

}  // namespace rc
