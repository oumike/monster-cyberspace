#include "app/app_state.h"
#include "app/ui_task.h"
#include "features.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#if RC_FEAT_FEED
#include "store/locked_feed.h"
#endif
#include "ui/home_items.h"
#include "ui/home_tiles.h"
#include "ui/tap_map.h"

#include <algorithm>
#include <string>

namespace rc {

namespace {
enum Item { kMessages, kUnread, kRooms, kCmail, kFeed, kGuilds, kSettings };

Item toItem(HomeItem h) {
  switch (h) {
    case HomeItem::Unread: return kUnread;
    case HomeItem::Messages: return kMessages;
    case HomeItem::Rooms: return kRooms;
    case HomeItem::Cmail: return kCmail;
    case HomeItem::Feed: return kFeed;
    case HomeItem::Guilds: return kGuilds;
    case HomeItem::Settings: return kSettings;
  }
  return kSettings;
}

// The tiles (touch boards), from the built features: unread spans the first
// row; messages shows the mentions.
const std::vector<Item>& tiles() {
  static const std::vector<Item> v = [] {
    std::vector<Item> out;
    for (HomeItem h : homeItems(kBuiltFeatures)) out.push_back(toItem(h));
    return out;
  }();
  return v;
}

// The list (keyed boards): the same items, messages first.
const std::vector<Item>& listItems() {
  static const std::vector<Item> v = [] {
    std::vector<Item> out;
    for (Item i : tiles())
      if (i != kUnread) out.push_back(i);
    if (kBuiltFeatures.unread) out.insert(out.begin() + 1, kUnread);
    return out;
  }();
  return v;
}
int tileCount() { return int(tiles().size()); }
constexpr int kTilePercent = 90;  // of the area between the status bar and the hint

#if RC_FEAT_FEED
// The newest unread Feed notice as an Entry arg ("postId\nnoticeId"), or "".
std::string newestNotice() {
  return lockedFeed().with([](FeedState& s) {
    if (s.notices.empty()) return std::string();
    const FeedNotice& n = s.notices.back();
    return n.postId + '\n' + n.id;
  });
}

int feedUnread() {
  return lockedFeed().with([](FeedState& s) { return int(s.notices.size()); });
}
#else
int feedUnread() { return 0; }
#endif

int unreadCount() {
#if RC_FEAT_UNREAD
  return unreadTotal(gatherUnread());
#else
  return 0;
#endif
}

int mailUnread() {
#if RC_FEAT_CMAIL
  return cmailUnread();
#else
  return 0;
#endif
}
}  // namespace

void HomeScreen::enter(const std::string&) {
  menu_.setSelected(0);
  tileSel_ = 0;
  tileHidden_ = ui::fromTouch();
  colTop_ = colCarry_ = 0;
}

void HomeScreen::openItem(int item) {
  switch (item) {
    case kMessages:
      router::go(router::Id::Room, app().cfg.lastRoom);
      break;
#if RC_FEAT_UNREAD
    case kUnread:
      router::go(router::Id::Notifications);
      break;
#endif
    case kRooms:
      router::go(router::Id::RoomList);
      break;
#if RC_FEAT_CMAIL
    case kCmail:
      router::go(router::Id::CmailList);
      break;
#endif
#if RC_FEAT_FEED
    case kFeed: {
      // With an unread notice, straight to its entry; else the list.
      std::string arg = newestNotice();
      if (arg.empty()) router::go(router::Id::FeedList);
      else router::go(router::Id::Entry, arg);
      break;
    }
#endif
#if RC_FEAT_GUILDS
    case kGuilds:
      router::go(router::Id::GuildList);
      break;
#endif
    case kSettings:
      router::go(router::Id::Settings);
      break;
    default:
      break;
  }
}

void HomeScreen::onKey(const KeyEvent& k) {
  if (tileMode_) {
    // Drags and taps hide the cursor and never move it; the next arrow or
    // Select only brings it back.
    if (ui::fromTouch()) {
      tileHidden_ = true;
      // In the column a drag sends Up/Down a row at a time: scroll by tiles.
      if (column_ && (k.action == Action::Up || k.action == Action::Down)) {
        colCarry_ += k.action == Action::Down ? 1 : -1;
        const int tiles = colCarry_ / colStride_;
        colCarry_ -= tiles * colStride_;
        colTop_ = std::max(0, std::min(colTop_ + tiles, tileCount() - colVisible_));
        return;
      }
      if (k.action != Action::Activate && k.action != Action::Select) return;
    } else if (tileHidden_) {
      switch (k.action) {
        case Action::Up:
        case Action::Down:
        case Action::Left:
        case Action::Right:
        case Action::Select:
          tileHidden_ = false;
          // The cursor comes back on a tile in view.
          if (column_ && (tileSel_ < colTop_ || tileSel_ >= colTop_ + colVisible_)) tileSel_ = colTop_;
          return;
        default:
          break;
      }
    }
    switch (k.action) {
      case Action::Up:
      case Action::Down:
      case Action::Left:
      case Action::Right:
        if (column_) {
          if (k.action == Action::Up || k.action == Action::Down)
            tileSel_ = std::max(0, std::min(tileCount() - 1, tileSel_ + (k.action == Action::Down ? 1 : -1)));
          // Keep the selection in view.
          colTop_ = std::min(colTop_, tileSel_);
          colTop_ = std::max(colTop_, tileSel_ - colVisible_ + 1);
        } else {
          tileSel_ = tileMove(tileSel_, tileCount(), k.action, true);
        }
        break;
      case Action::Select:
        openItem(tiles()[tileSel_]);
        break;
      case Action::Activate:
        if (k.target == uint8_t(ListId::Main) && k.index >= 0 && k.index < int(tiles().size())) {
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
      if (menu_.selected() >= 0 && menu_.selected() < int(listItems().size()))
        openItem(listItems()[menu_.selected()]);
      break;
    default:
      break;
  }
}

void HomeScreen::renderTiles(Grid& g, const Theme& t, const std::vector<TileRect>& tiles,
                             int cellW, int first, bool column) {
  const AppState& a = app();
  const std::vector<Item>& items = rc::tiles();
  const int count = int(items.size());
  if (tileSel_ < 0 || tileSel_ >= count) tileSel_ = 0;

  struct TileInfo {
    std::u32string label, sub;
    bool unread;
  };
  const int mentions = int(a.mentions.size()), cmail = mailUnread(), feed = feedUnread();
  const int unread = unreadCount();
  auto infoFor = [&](Item item) -> TileInfo {
    switch (item) {
      case kUnread:
        return {U"unread", utf8ToU32(std::to_string(unread) + " unread"), unread > 0};
      case kMessages:
        return {U"messages",
                mentions > 0
                    ? utf8ToU32(std::to_string(mentions) + (mentions == 1 ? " mention" : " mentions"))
                    : utf8ToU32("#" + a.cfg.lastRoom),
                mentions > 0};
      case kRooms:
        return {U"rooms", U"", false};
      case kCmail:
        return {U"c-mail", utf8ToU32(std::to_string(cmail) + " unread"), cmail > 0};
      case kFeed:
        return {U"feed", utf8ToU32(std::to_string(feed) + " unread"), feed > 0};
      case kGuilds:
        return {U"guilds", U"", false};
      case kSettings:
      default:
        return {U"settings", U"", false};
    }
  };

  if (activeTaps()) activeTaps()->setList(ListId::Main, tileHidden_ ? -1 : tileSel_, tileCount(), false);
  for (int slot = 0; slot < int(tiles.size()) && first + slot < tileCount(); ++slot) {
    const int i = first + slot;
    const TileRect& r = tiles[slot];
    const TileInfo it = infoFor(items[i]);
    const bool sel = i == tileSel_ && !tileHidden_;
    // The cursor is the outline alone: the selected tile's in the accent.
    const uint16_t bg = t.bg;
    for (int row = r.r0; row <= r.r1; ++row)
      for (int c = r.c0; c <= r.c1; ++c) g.put(c, row, U' ', bg, bg);

    const int mid = (r.r0 + r.r1) / 2;
    const int inW = r.c1 - r.c0 - 1;  // inside the outline
    const uint16_t labelFg = it.unread ? t.selectBg : t.fg;
    // A wide column tile has room for the sub line beside the label.
    const bool beside = column && !it.sub.empty() && mid + 1 > r.r1 - 1 &&
                        int(it.label.size() + 2 + it.sub.size()) <= inW;
    const int lineW = beside ? int(it.label.size() + 2 + it.sub.size()) : int(it.label.size());
    int lw = std::min<int>(int(it.label.size()), inW);
    const int lc = r.c0 + 1 + std::max(0, (inW - std::min(lineW, inW)) / 2);
    g.putText(lc, mid, it.label, labelFg, bg, lw);
    if (beside) g.putText(lc + lw + 2, mid, it.sub, t.dim, bg, int(it.sub.size()));
    if (!beside && !it.sub.empty() && mid + 1 <= r.r1 - 1) {
      const uint16_t subFg = t.dim;
      int sw = std::min<int>(int(it.sub.size()), inW);
      g.putText(r.c0 + 1 + std::max(0, (inW - sw) / 2), mid + 1, it.sub, subFg, bg, sw);
    }
    g.outline(r.c0, r.r0, r.c1, r.r1, sel && cursorShown() ? t.selectBg : t.dim);
    // Outlined themes have one ink for both outlines: the cursor is a second box.
    if (sel && cursorShown() && t.outlined && r.r1 - r.r0 >= 2 && r.c1 - r.c0 >= 2)
      g.outline(r.c0 + 1, r.r0 + 1, r.c1 - 1, r.r1 - 1, t.selectBg);

    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Main;
      tt.index = static_cast<int16_t>(i);
      // The column registers a row at a time, so a swipe's page of steps
      // (TapMap::visibleRows) is counted in rows, as drags are.
      if (column) {
        for (int row = r.r0; row <= r.r1; ++row) activeTaps()->addCells(r.c0, row, r.c1, row, tt, cellW);
      } else {
        activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt, cellW);
      }
    }
  }
  // More tiles above or below the column: arrows on the outline (e-paper only).
  if (column && board::eink() && !tiles.empty()) {
    const TileRect& top = tiles.front();
    const TileRect& bot = tiles.back();
    if (first > 0) g.put(top.c1 - 2, top.r0, U'\u25B2', t.fg, t.bg);
    if (first + int(tiles.size()) < tileCount()) g.put(bot.c1 - 2, bot.r1, U'\u25BC', t.fg, t.bg);
  }
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

void HomeScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  const AppState& a = app();

  // Touch boards: the tiles (ui/home_tiles.h), on RowKind::Big rows (the next
  // font up, +2 px) or, if those can't fit them, the grid's own rows.
  HomeLayout lay;
  bool big = false;
  if (board::hasTouch()) {
    const TextSize ts = a.cfg.textSize;
    const int areaPx = (g.rows() - 2) * cellSize(ts).h;
    // Gapped tiles first, big then normal rows; touching tiles only when no
    // gapped layout fits (the Wio Tracker L2 at Large).
    for (bool gaps : {true, false}) {
      for (bool tryBig : {true, false}) {
        lay = homeLayout(board::panelWidth(), areaPx, g.rows(), tryBig ? bigCellSize(ts) : cellSize(ts),
                         int(tiles().size()), kTilePercent, gaps, true);
        if (!lay.tiles.empty()) {
          big = tryBig;
          break;
        }
      }
      if (!lay.tiles.empty()) break;
    }
    // No grid fits (short panels at the larger sizes): one scrolling column,
    // gapped while that still shows three tiles, else touching.
    column_ = lay.tiles.empty();
    if (column_) {
      const int want = std::min(3, tileCount());
      HomeLayout best;
      bool bestBig = false;
      for (bool gaps : {true, false}) {
        for (bool tryBig : {true, false}) {
          HomeLayout c = homeColumn(board::panelWidth(), areaPx, g.rows(),
                                    tryBig ? bigCellSize(ts) : cellSize(ts), tileCount(), kTilePercent, gaps);
          if (c.tiles.size() > best.tiles.size()) {
            best = c;
            bestBig = tryBig;
          }
          if (int(c.tiles.size()) >= want) break;
        }
        if (int(best.tiles.size()) >= want) break;
      }
      lay = best;
      big = bestBig;
    }
  }
  const int cellW = big ? bigCellSize(a.cfg.textSize).w : 0;
  tileMode_ = !lay.tiles.empty();
  if (tileMode_) {
    for (int r = 2; r <= g.rows() - 2; ++r) {
      if (r > lay.lastRow) {
        g.setRowKind(r, RowKind::Skip);
        continue;
      }
      if (big) g.setRowKind(r, RowKind::Big);
      g.setRowShift(r, lay.shiftPx);
    }
    for (const auto& p : lay.pads) g.setRowPad(p.first, p.second);
    // @user stays in the status bar; the list-mode fields are dropped.
    drawStatus(g, t, a.user.empty() ? std::u32string(U"home") : utf8ToU32("@" + a.user), -1);
    int first = 0;
    if (column_) {
      colVisible_ = int(lay.tiles.size());
      colStride_ = colVisible_ > 1 ? lay.tiles[1].r0 - lay.tiles[0].r0 : 3;
      colTop_ = std::max(0, std::min(colTop_, tileCount() - colVisible_));
      first = colTop_;
    }
    renderTiles(g, t, lay.tiles, cellW, first, column_);
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
  const int mentions = int(a.mentions.size());
  field("mentions", std::to_string(mentions) + " unread", mentions ? t.selectBg : t.fg);
#if RC_FEAT_CMAIL
  int cmail = lockedChat().with([](ChatState& s) {
    int n = 0;
    for (const auto& c : s.conversations) n += c.unread;
    return n;
  });
  field("c-mail", std::to_string(cmail) + " unread", cmail ? t.selectBg : t.fg);
#endif
#if RC_FEAT_FEED
  int feed = feedUnread();
  field("feed", std::to_string(feed) + " unread", feed ? t.selectBg : t.fg);
#endif
  std::string active = lockedChat().with([](ChatState& s) {
    std::string out;
    for (const auto& room : s.rooms) {
      auto it = s.readAt.find(room.slug);
      if (it == s.readAt.end() || room.lastMessageAt > it->second) out += "#" + room.slug + " ";
    }
    return out.empty() ? std::string("none") : out;
  });
  field("active", active, t.fg);

  const int unread = unreadCount();
  std::vector<ListItem> rows;
  for (Item item : listItems()) {
    switch (item) {
      case kMessages: rows.push_back(ListItem{utf8ToU32("Messages  #" + a.cfg.lastRoom), U""}); break;
      case kUnread:
        rows.push_back(ListItem{utf8ToU32("Unread  " + std::to_string(unread)), U"", ItemTone::Normal, unread > 0});
        break;
      case kRooms: rows.push_back(ListItem{U"Rooms", U""}); break;
      case kCmail: rows.push_back(ListItem{U"C-Mail", U""}); break;
      case kFeed: rows.push_back(ListItem{U"Feed", U""}); break;
      case kGuilds: rows.push_back(ListItem{U"Guilds", U""}); break;
      case kSettings: rows.push_back(ListItem{U"Settings", U""}); break;
    }
  }
  const int nItems = int(rows.size());
  menu_.setItems(std::move(rows));
  int bottom = g.rows() - 1;
  // A short grid scrolls the menu rather than draw it over the fields.
  menu_.render(g, std::max(r, bottom - nItems), bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

}  // namespace rc
