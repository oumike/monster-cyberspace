#include "features.h"
#if RC_FEAT_UNREAD

#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "app/ui_task.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#include "store/locked_feed.h"
#include "store/settings.h"

namespace rc {

namespace {

const char* tag(UnreadKind k) {
  switch (k) {
    case UnreadKind::Cmail: return "mail  ";
    case UnreadKind::Mention: return "irc   ";
    case UnreadKind::Feed: return "feed  ";
  }
  return "";
}

}  // namespace

// The three sources as UnreadItems, each newest first, then merged. Each lock
// is short.
std::vector<UnreadItem> gatherUnread() {
  std::vector<UnreadItem> cmail, mentions, feed;
  lockedChat().with([&](ChatState& s) {
    for (const auto& c : s.conversations) {
      if (c.unread <= 0 || c.partnerDeleted) continue;
      cmail.push_back(UnreadItem{UnreadKind::Cmail, c.id, "@" + c.partner + ": " + c.lastMessage,
                                 c.lastMessageAt, c.unread});
    }
  });
  const auto& ms = app().mentions;
  for (auto it = ms.rbegin(); it != ms.rend(); ++it)
    mentions.push_back(UnreadItem{UnreadKind::Mention, it->slug, it->text, it->at, 1});
  lockedFeed().with([&](FeedState& s) {
    for (auto it = s.notices.rbegin(); it != s.notices.rend(); ++it)
      feed.push_back(UnreadItem{UnreadKind::Feed, it->postId + '\n' + it->id, it->text, it->at, 1});
  });
  return mergeUnread(std::move(cmail), std::move(mentions), std::move(feed));
}

void NotificationsScreen::askMarkAll() {
  if (marking_) return;
  confirm_ = true;
  box_.confirm(U"Mark all " + utf8ToU32(std::to_string(unreadTotal(items_))) +
                   U" read? (C-Mail, mentions and Feed, also on the website)",
               U"Mark all read");
}

void NotificationsScreen::open(int row) {
  if (row == 0 && !items_.empty()) return askMarkAll();
  const int index = row - 1;
  if (index < 0 || index >= int(items_.size())) return;
  const UnreadItem it = items_[index];
  using router::Id;
  switch (it.kind) {
    case UnreadKind::Cmail:
      router::goFrom(Id::Notifications, Id::Conversation, it.target);
      break;
    case UnreadKind::Mention: {
      // A mention without a room opens the last one; it is read once seen here.
      std::string slug = it.target;
      if (slug.empty()) {
        auto& ms = app().mentions;
        ms.erase(std::remove_if(ms.begin(), ms.end(), [](const MentionNote& m) { return m.slug.empty(); }),
                 ms.end());
        slug = app().cfg.lastRoom;
      }
      app().cfg.lastRoom = slug;
      settings::saveUiPrefs(app().cfg);
      router::goFrom(Id::Notifications, Id::Room, slug);
      break;
    }
    case UnreadKind::Feed:
      router::goFrom(Id::Notifications, Id::Entry, it.target);
      break;
  }
}

void NotificationsScreen::onKey(const KeyEvent& k) {
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
        chatSource().command(Command{CmdType::MarkAllRead});
        break;
      case Overlay::Result::Closed:
        confirm_ = false;
        break;
      case Overlay::Result::None:
        break;
    }
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

void NotificationsScreen::onEvent(const Event& e) {
  if (e.type == EvtType::AllRead) marking_ = false;
}

void NotificationsScreen::render(Grid& g, const Theme& t, int64_t) {
  items_ = gatherUnread();
  std::vector<ListItem> rows;
  rows.reserve(items_.size() + 1);
  if (!items_.empty())
    rows.push_back(ListItem{marking_ ? U"marking all read\u2026" : U"\u2713 Mark all read", U""});
  for (const auto& it : items_) {
    std::u32string right = it.kind == UnreadKind::Cmail ? utf8ToU32(std::to_string(it.count)) : U"";
    rows.push_back(ListItem{utf8ToU32(std::string(tag(it.kind)) + it.text), std::move(right),
                            ItemTone::Normal, true});
  }
  g.fill(t.bg);
  drawStatus(g, t, U"unread", unreadTotal(items_));
  const int bottom = g.rows() - 1;
  const int nav = sideNavWidth(g);
  list_.setItems(std::move(rows));
  list_.render(g, 1, bottom, 0, g.cols() - nav, t);
  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (items_.empty()) putLine(g, 1, 2, "nothing unread", t.dim, t.bg);
  if (confirm_) {
    box_.render(g, 1, bottom, nav, OverlayPlace::BottomRight, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF choose  back: cancel", U"tap: choose  outside: cancel");
    return;
  }
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
}

}  // namespace rc

#endif  // RC_FEAT_UNREAD
