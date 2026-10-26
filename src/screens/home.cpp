#include "app/app_state.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#include "store/locked_feed.h"

namespace rc {

namespace {
enum Item { kMessages, kRooms, kCmail, kFeed, kSettings };
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
  menu_.setSelected(0);
}

void HomeScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      menu_.up();
      break;
    case Action::Down:
      menu_.down();
      break;
    case Action::Select:
    case Action::Right:
      if (menu_.selected() == kMessages) {
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
      } else if (menu_.selected() == kSettings) {
        router::go(router::Id::Settings);
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

  menu_.setItems({
      ListItem{utf8ToU32("Messages  #" + a.cfg.lastRoom), U"", t.fg},
      ListItem{U"Rooms", U"", t.fg},
      ListItem{U"C-Mail", U"", t.fg},
      ListItem{U"Feed", U"", t.fg},
      ListItem{U"Settings", U"", t.fg},
  });
  int bottom = g.rows() - 1;
  menu_.render(g, bottom - kItems, bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open");
}

}  // namespace rc
