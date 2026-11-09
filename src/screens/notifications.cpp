#include "feature_flags.h"
#if RC_FEAT_UNREAD

#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "screens/button_pad.h"
#include "screens/lv_draw.h"
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

const char* kindName(UnreadKind k) {
  switch (k) {
    case UnreadKind::Cmail: return "C-Mail";
    case UnreadKind::Mention: return "Mention";
    case UnreadKind::Feed: return "Feed";
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

int countUnread() {
  int cmail = lockedChat().with([](ChatState& s) { return unreadCmailCount(s.conversations); });
  size_t notices = lockedFeed().with([](FeedState& s) { return s.notices.size(); });
  return cmail + unreadCount({}, app().mentions.size(), notices);
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

void NotificationsScreen::onEvent(const Event& e) {
  if (e.type == EvtType::AllRead) marking_ = false;
}

void NotificationsScreen::refreshRows(int64_t nowMs) {
  constexpr int64_t kRefreshMs = 1000;
  // Every touch board gets buttons, as the room list does; with keys, a cursor.
  const bool buttons = board::hasTouch();
  if (buttons != buttons_) shownCount_ = -1;
  buttons_ = buttons;
  pad_.setCursor(board::hasKeys());
  const int count = countUnread();
  if (count != shownCount_ || marking_ != shownMarking_ || nowMs - builtAt_ >= kRefreshMs) {
    shownCount_ = count;
    shownMarking_ = marking_;
    builtAt_ = nowMs;
    items_ = gatherUnread();
    rows_.clear();
    rows_.reserve(items_.size() + 1);
    if (!items_.empty())
      rows_.push_back(ListItem{marking_ ? U"marking all read\u2026" : U"\u2713 Mark all read", U""});
    for (const auto& it : items_) {
      std::u32string right = it.kind == UnreadKind::Cmail ? utf8ToU32(std::to_string(it.count)) : U"";
      ListItem row{utf8ToU32(std::string(tag(it.kind)) + it.text), std::move(right), ItemTone::Normal, true};
      // Buttons put the text on a line of its own under the kind.
      if (buttons_) {
        row.label = utf8ToU32(kindName(it.kind));
        row.detail = utf8ToU32(it.text);
      }
      rows_.push_back(std::move(row));
    }
    list_.setItems(rows_);  // keeps the selection, clamped
  }
}

void NotificationsScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->padView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  sync(0);
}

void NotificationsScreen::update(int64_t nowMs) {
  // C-Mail, mentions and notices change with an event (ConversationsUpdated,
  // MentionAlert, the Feed's), which ui::inputSeq() counts; edited text shows
  // with the gate's once-a-second refresh.
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().mentions.size())})) sync(nowMs);
}

void NotificationsScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  refreshRows(nowMs);
  syncStatus(v_->status_, U"unread", unreadTotal(items_));
  const int bottom = v_->text_.rows() - 1;
  const int nav = buttons_ ? 0 : sideNavWidth(v_->text_);
  lvui::setHidden(v_->listView_.obj(), buttons_);
  lvui::setHidden(v_->padView_.obj(), !buttons_);
  if (buttons_) {
    pad_.sync(v_->padView_, rows_, 2, bottom, true, 1);
  } else {
    placeCells(v_->listView_.obj(), 0, 1, v_->text_.cols() - nav, bottom);
    v_->listView_.sync(list_);
  }
  v_->navView_.sync(nav, 1, bottom, v_->text_.cols());
  v_->text_.begin();
  if (items_.empty()) v_->text_.line(1, 2, "nothing unread", t.dim);
  v_->text_.end();
  const lvui::Cell c = v_->text_.cell();
  v_->menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, nav * c.w, c.h);
  v_->menuView_.sync(box_);
  if (confirm_)
    return syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: cancel", U"tap: choose  outside: cancel");
  syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home",
           buttons_ ? U"tap to open  drag to scroll" : U"tap to open  swipe to scroll");
}

}  // namespace rc

#endif  // RC_FEAT_UNREAD
