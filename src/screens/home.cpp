#include <Arduino.h>

#include "app/app_state.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#include "store/locked_feed.h"

namespace rc {

namespace {
enum Item { kMessages, kRooms, kCmail, kFeed, kReset };
constexpr int kItems = 5;

// The newest unread Feed notice as an Entry arg ("postId\nnoticeId"), or "".
std::string newestNotice() {
  return lockedFeed().with([](FeedState& s) {
    if (s.notices.empty()) return std::string();
    const FeedNotice& n = s.notices.back();
    return n.postId + '\n' + n.id;
  });
}
}

void HomeScreen::enter(const std::string&) {
  confirm_ = false;
  menu_.setSelected(0);
}

void HomeScreen::onKey(const KeyEvent& k) {
  ListView& m = confirm_ ? confirmMenu_ : menu_;
  switch (k.action) {
    case Action::Up:
      m.up();
      break;
    case Action::Down:
      m.down();
      break;
    case Action::Back:
    case Action::Left:
      confirm_ = false;
      break;
    case Action::Select:
    case Action::Right:
      if (confirm_) {
        if (confirmMenu_.selected() == 1) {
          settings::factoryReset();
          delay(100);
          ESP.restart();
        }
        confirm_ = false;
      } else if (menu_.selected() == kMessages) {
        router::go(router::Id::Room, app().cfg.lastRoom);
      } else if (menu_.selected() == kRooms) {
        router::go(router::Id::RoomList);
      } else if (menu_.selected() == kCmail) {
        router::go(router::Id::CmailList);
      } else if (menu_.selected() == kFeed) {
        // With an unread notice, straight to its entry; else the list.
        std::string arg = newestNotice();
        if (arg.empty()) router::go(router::Id::FeedList);
        else router::go(router::Id::Entry, arg);
      } else if (menu_.selected() == kReset) {
        confirm_ = true;
        confirmMenu_.setSelected(0);
      }
      break;
    default:
      break;
  }
}

void HomeScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"home", -1);
  const AppState& a = app();
  int r = 1;
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    putLine(g, 1, r, label, t.dim, t.bg);
    putLine(g, 11, r++, value, fg, t.bg);
  };
  field("user", a.user.empty() ? "not signed in" : "@" + a.user + (a.auth == 2 ? " (unverified)" : ""),
        t.fg);
  field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
  field("settings", "http://rhino-cyberspace.local", t.fg);
  field("mentions", std::to_string(a.unreadMentions) + " unread", a.unreadMentions ? t.selectBg : t.fg);
  int cmail = lockedChat().with([](ChatState& s) {
    int n = 0;
    for (const auto& c : s.conversations) n += c.unread;
    return n;
  });
  field("c-mail", std::to_string(cmail) + " unread", cmail ? t.selectBg : t.fg);
  int feed = lockedFeed().with([](FeedState& s) { return int(s.notices.size()); });
  field("feed", std::to_string(feed) + " unread", feed ? t.selectBg : t.fg);
  std::string active = lockedChat().with([](ChatState& s) {
    std::string out;
    for (const auto& room : s.rooms) {
      auto it = s.readAt.find(room.slug);
      if (it == s.readAt.end() || room.lastMessageAt > it->second) out += "#" + room.slug + " ";
    }
    return out.empty() ? std::string("none") : out;
  });
  field("active", active, t.fg);
  int bat = board::batteryPercent();
  field("battery", bat < 0 ? "unknown" : std::to_string(bat) + "%", t.fg);

  menu_.setItems({
      ListItem{utf8ToU32("Messages  #" + a.cfg.lastRoom), U"", t.fg},
      ListItem{U"Rooms", U"", t.fg},
      ListItem{U"C-Mail", U"", t.fg},
      ListItem{U"Feed", U"", t.fg},
      ListItem{U"Factory reset\u2026", U"", t.error},
  });
  int bottom = g.rows() - 1;
  if (confirm_) {
    confirmMenu_.setItems({ListItem{U"Cancel", U"", t.fg},
                           ListItem{U"Erase everything", U"", t.error}});
    putLine(g, 1, bottom - 3, "Erase Wi-Fi, account and settings?", t.error, t.bg);
    confirmMenu_.render(g, bottom - 2, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel");
  } else {
    menu_.render(g, bottom - kItems, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF open");
  }
}

}  // namespace rc
