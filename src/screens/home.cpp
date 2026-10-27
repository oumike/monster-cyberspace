#include "app/app_state.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#include "store/locked_feed.h"
#include "ui/home_tiles.h"
#include "ui/tap_map.h"

#include <algorithm>
#include <string>

namespace rc {

namespace {
enum Item { kMessages, kRooms, kCmail, kFeed, kSettings, kMentions };
constexpr int kItems = 5;      // the list (keyed boards)
constexpr int kTileCount = 6;  // the tiles (touch boards)
constexpr int kTilePercent = 90;  // of the area between the status bar and the hint

// The newest unread Feed notice as an Entry arg ("postId\nnoticeId"), or "".
std::string newestNotice() {
  return lockedFeed().with([](FeedState& s) {
    if (s.notices.empty()) return std::string();
    const FeedNotice& n = s.notices.back();
    return n.postId + '\n' + n.id;
  });
}

int cmailUnread() {
  return lockedChat().with([](ChatState& s) {
    int n = 0;
    for (const auto& c : s.conversations) n += c.unread;
    return n;
  });
}

int feedUnread() {
  return lockedFeed().with([](FeedState& s) { return int(s.notices.size()); });
}
}  // namespace

void HomeScreen::enter(const std::string&) {
  menu_.setSelected(0);
  tileSel_ = 0;
}

void HomeScreen::openItem(int item) {
  switch (item) {
    case kMessages:
    case kMentions:
      router::go(router::Id::Room, app().cfg.lastRoom);
      break;
    case kRooms:
      router::go(router::Id::RoomList);
      break;
    case kCmail:
      router::go(router::Id::CmailList);
      break;
    case kFeed: {
      // With an unread notice, straight to its entry; else the list.
      std::string arg = newestNotice();
      if (arg.empty()) router::go(router::Id::FeedList);
      else router::go(router::Id::Entry, arg);
      break;
    }
    case kSettings:
      router::go(router::Id::Settings);
      break;
    default:
      break;
  }
}

void HomeScreen::onKey(const KeyEvent& k) {
  if (tileMode_) {
    switch (k.action) {
      case Action::Up:
      case Action::Down:
      case Action::Left:
      case Action::Right:
        tileSel_ = tileMove(tileSel_, kTileCount, k.action);
        break;
      case Action::Select:
        openItem(tileSel_);
        break;
      case Action::Activate:
        if (k.target == uint8_t(ListId::Main) && k.index >= 0 && k.index < kTileCount) {
          tileSel_ = k.index;
          onKey(KeyEvent{k.then});
        }
        break;
      default:
        break;
    }
    return;
  }
  switch (k.action) {
    case Action::Up:
      menu_.up();
      break;
    case Action::Down:
      menu_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && menu_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right:
      openItem(menu_.selected());
      break;
    default:
      break;
  }
}

void HomeScreen::renderTiles(Grid& g, const Theme& t, const std::vector<TileRect>& tiles,
                             int cellW) {
  const AppState& a = app();
  if (tileSel_ < 0 || tileSel_ >= kTileCount) tileSel_ = 0;

  struct TileInfo {
    std::u32string label, sub;
    bool unread;
  };
  int mentions = a.unreadMentions, cmail = cmailUnread(), feed = feedUnread();
  TileInfo info[kTileCount] = {
      {U"messages", utf8ToU32("#" + a.cfg.lastRoom), false},
      {U"rooms", U"", false},
      {U"c-mail", utf8ToU32(std::to_string(cmail) + " unread"), cmail > 0},
      {U"feed", utf8ToU32(std::to_string(feed) + " unread"), feed > 0},
      {U"settings", U"", false},
      {utf8ToU32(std::to_string(mentions) + " mentions"), U"", mentions > 0},
  };

  if (activeTaps()) activeTaps()->setList(ListId::Main, tileSel_, kTileCount, false);
  for (int i = 0; i < kTileCount && i < int(tiles.size()); ++i) {
    const TileRect& r = tiles[i];
    const TileInfo& it = info[i];
    const bool sel = i == tileSel_;
    const uint16_t bg = sel ? t.selectBg : t.bg;
    for (int row = r.r0; row <= r.r1; ++row)
      for (int c = r.c0; c <= r.c1; ++c) g.put(c, row, U' ', bg, bg);

    const int mid = (r.r0 + r.r1) / 2;
    const int inW = r.c1 - r.c0 - 1;  // inside the outline
    const uint16_t labelFg = sel ? t.selectFg : (it.unread ? t.selectBg : t.fg);
    int lw = std::min<int>(int(it.label.size()), inW);
    g.putText(r.c0 + 1 + std::max(0, (inW - lw) / 2), mid, it.label, labelFg, bg, lw);
    if (!it.sub.empty() && mid + 1 <= r.r1 - 1) {
      const uint16_t subFg = sel ? t.selectFg : t.dim;
      int sw = std::min<int>(int(it.sub.size()), inW);
      g.putText(r.c0 + 1 + std::max(0, (inW - sw) / 2), mid + 1, it.sub, subFg, bg, sw);
    }
    g.outline(r.c0, r.r0, r.c1, r.r1, sel ? t.selectBg : t.dim);

    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Main;
      tt.index = static_cast<int16_t>(i);
      activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt, cellW);
    }
  }
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

void HomeScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  const AppState& a = app();

  // Touch boards: the tiles take kTilePercent of the space between the status
  // bar and the hint, on RowKind::Big rows (the next font up, +2 px) or, if
  // those can't fit them, the grid's own rows. Row 1 is a blank pad row that
  // makes the space above the tiles match the space below.
  std::vector<TileRect> tiles;
  bool big = false;
  TileRows tr{0, 0};
  if (board::hasTouch() && g.rows() > 3) {
    const TextSize ts = a.cfg.textSize;
    const int areaPx = (g.rows() - 2) * cellSize(ts).h;
    for (bool tryBig : {true, false}) {
      const int rowH = tryBig ? bigCellSize(ts).h : cellSize(ts).h;
      const int cols = tryBig ? bigCols(board::panelWidth(), ts) : g.cols();
      tr = tileRows(areaPx, rowH, kTilePercent);
      // Tiles on rows 1..tr.rows, then shifted down past the pad row.
      tiles = homeTiles(cols, std::min(tr.rows, g.rows() - 3) + 2, kTileCount, kTilePercent, 100);
      if (!tiles.empty()) {
        big = tryBig;
        break;
      }
    }
    for (TileRect& r : tiles) ++r.r0, ++r.r1;
  }
  const int cellW = big ? bigCellSize(a.cfg.textSize).w : 0;
  tileMode_ = !tiles.empty();
  if (tileMode_) {
    g.setRowPad(1, tr.padPx);
    for (int r = 2; r <= g.rows() - 2; ++r) {
      if (r > tr.rows + 1) g.setRowKind(r, RowKind::Skip);
      else if (big) g.setRowKind(r, RowKind::Big);
    }
    // @user stays in the status bar; the list-mode fields are dropped.
    drawStatus(g, t, a.user.empty() ? std::u32string(U"home") : utf8ToU32("@" + a.user), -1);
    renderTiles(g, t, tiles, cellW);
    return;
  }

  drawStatus(g, t, U"home", -1);
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
      ListItem{utf8ToU32("Messages  #" + a.cfg.lastRoom), U""},
      ListItem{U"Rooms", U""},
      ListItem{U"C-Mail", U""},
      ListItem{U"Feed", U""},
      ListItem{U"Settings", U""},
  });
  int bottom = g.rows() - 1;
  menu_.render(g, bottom - kItems, bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

}  // namespace rc
