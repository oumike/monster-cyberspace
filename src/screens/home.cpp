#include "app/app_state.h"
#include "app/ui_task.h"
#include "feature_flags.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/lv_draw.h"
#if RC_FEAT_PEOPLE
#include "screens/people_nav.h"
#endif
#include "screens/screens.h"
#include "store/locked_chat.h"
#if RC_FEAT_FEED
#include "store/locked_feed.h"
#endif
#include "ui/home_items.h"
#include "ui/home_tiles.h"
#include "ui/tap_map.h"
#include "ui/unread_inbox.h"

#include <algorithm>
#include <functional>
#include <string>

namespace rc {

namespace {
enum Item { kMessages, kUnread, kRooms, kCmail, kFeed, kFollowing, kGuilds, kProfile, kSettings };

Item toItem(HomeItem h) {
  switch (h) {
    case HomeItem::Unread: return kUnread;
    case HomeItem::Messages: return kMessages;
    case HomeItem::Rooms: return kRooms;
    case HomeItem::Cmail: return kCmail;
    case HomeItem::Feed: return kFeed;
    case HomeItem::Following: return kFollowing;
    case HomeItem::Guilds: return kGuilds;
    case HomeItem::Profile: return kProfile;
    case HomeItem::Settings: return kSettings;
  }
  return kSettings;
}

// The tiles (touch boards), from the built features: unread spans the first
// row when spanFirst(); messages shows the mentions; profile is yours.
const std::vector<Item>& tiles() {
  static const std::vector<Item> v = [] {
    std::vector<Item> out;
    for (HomeItem h : homeItems(kBuiltFeatures)) out.push_back(toItem(h));
    return out;
  }();
  return v;
}

// The list (keyed boards): the same items, messages (with cIRC) first.
const std::vector<Item>& listItems() {
  static const std::vector<Item> v = [] {
    std::vector<Item> out;
    for (Item i : tiles())
      if (i != kUnread) out.push_back(i);
    if (kBuiltFeatures.unread) out.insert(out.begin() + (kBuiltFeatures.circ ? 1 : 0), kUnread);
    return out;
  }();
  return v;
}
int tileCount() { return int(tiles().size()); }
// Unread spans the first row only when the rest fill their rows of two.
bool spanFirst() {
  static const bool v = homeSpanFirst(homeItems(kBuiltFeatures));
  return v;
}
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
  return countUnread();  // no copies: this runs every frame
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

// What Home shows from the chat state, as scalars, under one lock: the C-Mail
// unread sum and conversations with unread (unreadCmailCount), and with
// `active` (list mode) a mask of the rooms the "active" field lists.
struct ChatInputs {
  int64_t unread = 0, conversations = 0, active = 0;
};
ChatInputs chatInputs(bool active) {
  const int64_t since = nowEpochMs() - 8LL * 60 * 60 * 1000;
  return lockedChat().with([&](ChatState& s) {
    ChatInputs in;
    for (const auto& c : s.conversations) in.unread += c.unread;
    in.conversations = unreadCmailCount(s.conversations);
    if (active)
      for (size_t i = 0; i < s.rooms.size(); ++i)
        if (s.rooms[i].lastMessageAt > 0 && s.rooms[i].lastMessageAt >= since) in.active |= int64_t(1) << (i % 63);
    return in;
  });
}

// Every tile's text, in tiles() order.
std::vector<lvui::HomeTileInfo> tileInfos() {
  const AppState& a = app();
  const int mentions = int(a.mentions.size()), cmail = mailUnread(), feed = feedUnread();
  const int unread = unreadCount();
  auto infoFor = [&](Item item) -> lvui::HomeTileInfo {
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
      case kFollowing:
        return {U"following", U"", false};
      case kGuilds:
        return {U"guilds", U"", false};
      case kProfile:
        return {U"profile", a.user.empty() ? std::u32string() : utf8ToU32("@" + a.user), false};
      case kSettings:
      default:
        return {U"settings", U"", false};
    }
  };
  std::vector<lvui::HomeTileInfo> out;
  for (Item item : tiles()) out.push_back(infoFor(item));
  return out;
}

// List mode's lines above the menu: a dim label and its value.
struct Field {
  const char* label;
  std::string value;
  uint16_t fg;
};
std::vector<Field> listFields(const Theme& t) {
  const AppState& a = app();
  std::vector<Field> out;
  out.push_back({"user", a.user.empty() ? "not signed in" : "@" + a.user + (a.auth == 2 ? " (unverified)" : ""),
                 t.fg});
  const int mentions = int(a.mentions.size());
  out.push_back({"mentions", std::to_string(mentions) + " unread", mentions ? t.selectBg : t.fg});
#if RC_FEAT_CMAIL
  int cmail = lockedChat().with([](ChatState& s) {
    int n = 0;
    for (const auto& c : s.conversations) n += c.unread;
    return n;
  });
  out.push_back({"c-mail", std::to_string(cmail) + " unread", cmail ? t.selectBg : t.fg});
#endif
#if RC_FEAT_FEED
  int feed = feedUnread();
  out.push_back({"feed", std::to_string(feed) + " unread", feed ? t.selectBg : t.fg});
#endif
  // Rooms with a message in the last 8 hours, read or not (every room that has
  // messages until the clock is set).
  const int64_t since = nowEpochMs() - 8LL * 60 * 60 * 1000;
  std::string active = lockedChat().with([since](ChatState& s) {
    std::string out;
    for (const auto& room : s.rooms)
      if (room.lastMessageAt > 0 && room.lastMessageAt >= since) out += "#" + room.slug + " ";
    return out.empty() ? std::string("none") : out;
  });
  out.push_back({"active", active, t.fg});
  return out;
}

// List mode's menu rows, in listItems() order.
std::vector<ListItem> menuRows() {
  const AppState& a = app();
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
      case kFollowing: rows.push_back(ListItem{U"Following", U""}); break;
      case kGuilds: rows.push_back(ListItem{U"Guilds", U""}); break;
      case kProfile: rows.push_back(ListItem{U"Profile", U""}); break;
      case kSettings: rows.push_back(ListItem{U"Settings", U""}); break;
    }
  }
  return rows;
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
#if RC_FEAT_CIRC
    case kMessages:
      router::go(router::Id::Room, app().cfg.lastRoom);
      break;
    case kRooms:
      router::go(router::Id::RoomList);
      break;
#endif
#if RC_FEAT_UNREAD
    case kUnread:
      router::go(router::Id::Notifications);
      break;
#endif
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
#if RC_FEAT_PEOPLE && RC_FEAT_FEED
    case kFollowing:
      router::go(router::Id::Following);
      break;
#endif
#if RC_FEAT_GUILDS
    case kGuilds:
      router::go(router::Id::GuildList);
      break;
#endif
#if RC_FEAT_PEOPLE
    case kProfile:
      // Your own profile: its Entries item lists your posts.
      if (app().user.empty()) ui::banner("Not signed in", true);
      else people::openProfile(app().user);
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
          tileSel_ = tileMove(tileSel_, tileCount(), k.action, spanFirst());
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
  const int count = tileCount();
  if (tileSel_ < 0 || tileSel_ >= count) tileSel_ = 0;

  const std::vector<lvui::HomeTileInfo> infos = tileInfos();

  if (activeTaps()) activeTaps()->setList(ListId::Main, tileHidden_ ? -1 : tileSel_, tileCount(), false);
  for (int slot = 0; slot < int(tiles.size()) && first + slot < tileCount(); ++slot) {
    const int i = first + slot;
    const TileRect& r = tiles[slot];
    const lvui::HomeTileInfo& it = infos[i];
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
                         int(tiles().size()), kTilePercent, gaps, spanFirst());
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
  for (const Field& f : listFields(t)) {
    putLine(g, 1, r, f.label, t.dim, t.bg);
    putLine(g, 11, r++, f.value, f.fg, t.bg);
  }

  std::vector<ListItem> rows = menuRows();
  const int nItems = int(rows.size());
  menu_.setItems(std::move(rows));
  int bottom = g.rows() - 1;
  // A short grid scrolls the menu rather than draw it over the fields.
  menu_.render(g, std::max(r, bottom - nItems), bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

void HomeScreen::build(lv_obj_t* root) {
  root_ = root;
  text_.create(root);
  menuView_.create(root);
  tilesView_.create(root);
  hint_.create(root);
  status_.create(root);
  sync();
}

void HomeScreen::update(int64_t nowMs) {
  const AppState& a = app();
  const ChatInputs chat = chatInputs(!tileMode_);
  const std::hash<std::string> hash;
  const int64_t cursor = (int64_t(tileSel_) << 40) ^ (int64_t(colTop_) << 20) ^ (int64_t(menu_.selected()) << 1) ^
                         int64_t(tileHidden_);
  if (gate_.due(root_, nowMs,
                {int64_t(a.mentions.size()), chat.unread, chat.conversations, chat.active, feedUnread(),
                 int64_t(hash(a.user)), int64_t(hash(a.cfg.lastRoom)), cursor}))
    sync();
}

void HomeScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const AppState& a = app();
  const lvui::Cell c = lvui::cellOf(root_);
  const int rows = text_.rows(), bottom = rows - 1;

  // Touch boards: the tiles, laid out as render lays them out (lvui::homeFit).
  lvui::HomeFit fit;
  if (board::hasTouch()) {
    const TextSize ts = a.cfg.textSize;
    fit = lvui::homeFit(board::panelWidth(), (rows - 2) * cellSize(ts).h, rows, ts, tileCount(), kTilePercent,
                        spanFirst());
    column_ = fit.column;
  }
  tileMode_ = !fit.lay.tiles.empty();
  lvui::setHidden(tilesView_.obj(), !tileMode_);
  lvui::setHidden(menuView_.obj(), tileMode_);
  text_.begin();
  if (tileMode_) {
    text_.end();
    syncStatus(status_, a.user.empty() ? std::u32string(U"home") : utf8ToU32("@" + a.user), -1);
    int first = 0;
    if (column_) {
      colVisible_ = int(fit.lay.tiles.size());
      colStride_ = colVisible_ > 1 ? fit.lay.tiles[1].r0 - fit.lay.tiles[0].r0 : 3;
      colTop_ = std::max(0, std::min(colTop_, tileCount() - colVisible_));
      first = colTop_;
    }
    if (tileSel_ < 0 || tileSel_ >= tileCount()) tileSel_ = 0;
    tilesView_.syncHome(fit, 1 * c.h, tileInfos(), first, tileHidden_ ? -1 : tileSel_, board::eink());
    syncHint(hint_, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
    return;
  }

  syncStatus(status_, U"home", -1);
  int r = 1;
  for (const Field& f : listFields(t)) {
    text_.line(1, r, f.label, t.dim);
    text_.line(11, r++, f.value, f.fg);
  }
  text_.end();
  std::vector<ListItem> items = menuRows();
  const int nItems = int(items.size());
  menu_.setItems(std::move(items));
  // A short grid scrolls the menu rather than draw it over the fields.
  placeCells(menuView_.obj(), 1, std::max(r, bottom - nItems), text_.cols() - 2, bottom);
  menuView_.sync(menu_);
  syncHint(hint_, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

}  // namespace rc
