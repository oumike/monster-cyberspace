#include "app/app_state.h"
#include "app/ui_task.h"
#include "feature_flags.h"
#include "screens/status_lines.h"
#include "hal/board.h"
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
#include "ui/unread_inbox.h"

#include <algorithm>
#include <functional>
#include <string>

namespace rc {

namespace {
enum Item {
  kMessages, kUnread, kRooms, kCmail, kFeed, kFollowing, kGuilds, kProfile, kSettings, kNotes, kPrograms,
  kSearch
};

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
    case HomeItem::Notes: return kNotes;
    case HomeItem::Programs: return kPrograms;
    case HomeItem::Search: return kSearch;
  }
  return kSettings;
}

// The buttons (touch boards), from the built features; profile is yours.
// Where Home lists the unread rows itself (homeShowsStatus) there is no
// Unread button, so Messages and Rooms take the top row.
const std::vector<Item>& tiles() {
  static const std::vector<Item> all = [] {
    std::vector<Item> out;
    for (HomeItem h : homeItems(kBuiltFeatures)) out.push_back(toItem(h));
    return out;
  }();
  static const std::vector<Item> noUnread = [] {
    std::vector<Item> out;
    for (Item i : all)
      if (i != kUnread) out.push_back(i);
    return out;
  }();
  return homeShowsStatus() ? noUnread : all;
}

// The list (keyed boards): the same items, messages (with cIRC) first.
const std::vector<Item>& listItems() {
  static const std::vector<Item> all = [] {
    std::vector<Item> out;
    for (HomeItem h : homeItems(kBuiltFeatures))
      if (h != HomeItem::Unread) out.push_back(toItem(h));
    if (kBuiltFeatures.unread) out.insert(out.begin() + (kBuiltFeatures.circ ? 1 : 0), kUnread);
    return out;
  }();
  static const std::vector<Item> noUnread = [] {
    std::vector<Item> out;
    for (Item i : all)
      if (i != kUnread) out.push_back(i);
    return out;
  }();
  return homeShowsStatus() ? noUnread : all;
}
int tileCount() { return int(tiles().size()); }
constexpr int kTilePercent = 90;  // of the area between the status bar and the hint

#if RC_FEAT_FEED
// The newest unread notice about an entry as an Entry arg ("postId\nnoticeId"),
// or "". Followers and pokes open profiles from Unread instead.
std::string newestNotice() {
  return lockedFeed().with([](FeedState& s) {
    for (auto it = s.notices.rbegin(); it != s.notices.rend(); ++it)
      if (!it->postId.empty()) return it->postId + '\n' + it->id;
    return std::string();
  });
}

int feedUnread() {
  return lockedFeed().with([](FeedState& s) {
    return int(std::count_if(s.notices.begin(), s.notices.end(),
                             [](const FeedNotice& n) { return !n.postId.empty(); }));
  });
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
// unread message sum (so a new message redraws) and conversations with unread (unreadCmailCount), and with
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

// Every button's label, in tiles() order: unread where something waits.
std::vector<lvui::HomeTileInfo> tileInfos() {
  const AppState& a = app();
  const int mentions = int(a.mentions.size()), cmail = mailUnread(), feed = feedUnread();
  const int unread = unreadCount();
  auto infoFor = [&](Item item) -> lvui::HomeTileInfo {
    switch (item) {
      case kUnread: return {U"unread", unread > 0};
      case kMessages: return {U"messages", mentions > 0};
      case kRooms: return {U"rooms", false};
      case kCmail: return {U"c-mail", cmail > 0};
      case kFeed: return {U"feed", feed > 0};
      case kFollowing: return {U"following", false};
      case kNotes: return {U"notes", false};
      case kGuilds: return {U"guilds", false};
      case kPrograms: return {U"programs", false};
      case kSearch: return {U"search", false};
      case kProfile: return {U"profile", false};
      case kSettings:
      default: return {U"settings", false};
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
  int cmail = cmailUnread();
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
      case kNotes: rows.push_back(ListItem{U"Notes", U""}); break;
      case kGuilds: rows.push_back(ListItem{U"Guilds", U""}); break;
      case kPrograms: rows.push_back(ListItem{U"Programs", U""}); break;
      case kSearch: rows.push_back(ListItem{U"Search", U""}); break;
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
#if RC_FEAT_NOTES
    case kNotes:
      router::go(router::Id::Notes);
      break;
#endif
#if RC_FEAT_PROGRAMS
    case kPrograms:
      router::go(router::Id::Programs);
      break;
#endif
#if RC_FEAT_SEARCH
    case kSearch:
      SearchScreen::markReturn(false);  // from Home: a fresh query line
      router::go(router::Id::Search);
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
#if RC_FEAT_UNREAD
  // A tap on an unread row opens it; a drag only moves the selection.
  if (k.action == Action::Activate && k.target == uint8_t(ListId::Aside)) {
    if (unread_.select(k.index) && k.then != Action::None && k.index >= 0 &&
        k.index < int(unreadItems_.size())) {
      const UnreadItem it = unreadItems_[k.index];
      openUnread(it, router::Id::Home);
    }
    return;
  }
#endif
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
      // Elsewhere a drag scrolls the page a row at a time.
      if (k.action == Action::Up || k.action == Action::Down) {
        pageTop_ = std::max(0, std::min(pageTop_ + (k.action == Action::Down ? 1 : -1), pageMax_));
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
          pageTop_ = 0;
          // The cursor comes back on a tile in view.
          if (column_ && (tileSel_ < colTop_ || tileSel_ >= colTop_ + colVisible_)) tileSel_ = colTop_;
          return;
        default:
          break;
      }
    }
    // The buttons fit the screen: keys bring the page back to its top.
    if (!ui::fromTouch()) pageTop_ = 0;
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
          tileSel_ = int(btnRects_.size()) == tileCount() ? buttonMove(btnRects_, tileSel_, k.action)
                                                         : tileMove(tileSel_, tileCount(), k.action);
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

void HomeScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->root_ = root;
  v_->text_.create(root);
  v_->menuView_.create(root);
  v_->page_ = lvui::plainBox(root);
  v_->tilesView_.create(v_->page_);
  v_->unreadView_.create(v_->page_);
  v_->unreadView_.setListId(ListId::Aside);
  v_->unreadView_.setDragKeys(true);
  unread_.setListId(ListId::Aside);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync();
}

void HomeScreen::update(int64_t nowMs) {
  const AppState& a = app();
  const ChatInputs chat = chatInputs(!tileMode_);
  const std::hash<std::string> hash;
  const int64_t cursor = (int64_t(tileSel_) << 40) ^ (int64_t(colTop_) << 20) ^ (int64_t(menu_.selected()) << 1) ^
                         int64_t(tileHidden_) ^ (int64_t(pageTop_) << 52);
  if (v_->gate_.due(v_->root_, nowMs,
                {int64_t(a.mentions.size()), chat.unread, chat.conversations, chat.active, feedUnread(),
                 int64_t(hash(a.user)), int64_t(hash(a.cfg.lastRoom)), cursor}))
    sync();
}

namespace {
// Text on the page: rows drawn `shift` up, only those in [top, end) of the screen.
struct PageText {
  lvui::CellText& text;
  int shift, top, end;
  template <class S>
  void line(int col, int row, const S& s, uint16_t fg, int maxCols = -1) const {
    row -= shift;
    if (row >= top && row < end) text.line(col, row, s, fg, maxCols);
  }
};

// The status lines (status_lines.h) in a box of `width` cells from (col, row),
// wrapped, stopping above row `end`.
int drawStatus(const PageText& text, const Theme& t, int col, int row, int width, int end) {
  constexpr int kLabelW = 9;  // "setup AP" and a space
  if (width <= kLabelW + 4 || row >= end) return 0;
  const int valueW = width - kLabelW;
  const int n = layoutStatus(statusFields(t, setupApUp(), timesvc::snapshot()), true, valueW, 0, end - row + 1,
               [&](int r, const char* label, const std::u32string& line, uint16_t fg) {
                 const int at = row + r - 1;
                 if (label && *label) text.line(col, at, std::string_view(label), t.dim, kLabelW);
                 text.line(col + kLabelW, at, line, fg, valueW);
               });
  return std::min(n, end - row);
}

// Fluffy, or the custom mascot when one is set.
const std::vector<std::string>& mascotArt() {
  const std::vector<std::string>& custom = app().mascot.art;
  const bool own = std::any_of(custom.begin(), custom.end(),
                               [](const std::string& l) { return l.find_first_not_of(' ') != std::string::npos; });
  return own ? custom : defaultMascotArt();
}

int mascotWidth(const std::vector<std::string>& art) {
  int w = 0;
  for (const std::string& l : art) w = std::max(w, int(utf8ToU32(l).size()));
  return w;
}

bool mascotFits(int width, int height) {
  const std::vector<std::string>& art = mascotArt();
  return mascotWidth(art) <= width && int(art.size()) <= height;
}

// The mascot centred in a box of width x height cells from (col, row), when it fits.
void drawMascot(const PageText& text, const Theme& t, int col, int row, int width, int height) {
  const std::vector<std::string>& art = mascotArt();
  const int w = mascotWidth(art);
  const int h = int(art.size());
  if (w > width || h > height) return;
  const int c0 = col + (width - w) / 2, r0 = row + (height - h) / 2;
  for (int i = 0; i < h; ++i) text.line(c0, r0 + i, art[i], t.selectBg);
}
}  // namespace

int HomeScreen::syncAside(int col, int row, int width, int end, int shift) {
  const Theme& t = lvui::currentTheme();
  const PageText pg{v_->text_, shift, 1, v_->text_.rows() - 1};
  int r = row + drawStatus(pg, t, col, row, width, end);
  bool list = false;
#if RC_FEAT_UNREAD
  if (r + 2 < end) {
    pg.line(col, r++, std::u32string(size_t(width), U'\u2500'), t.dim);
    if (unreadItems_.empty()) {
      pg.line(col, r++, "nothing unread", t.dim, width);
    } else {
      const int stop = std::min(end, r + int(unreadItems_.size()));
      // The page starts at screen row 1.
      const lvui::Cell c = lvui::cellOf(v_->page_);
      lvui::place(v_->unreadView_.obj(), col * c.w, (r - 1 - shift) * c.h, width * c.w, (stop - r) * c.h);
      v_->unreadView_.sync(unread_);
      list = true;
      r = stop;
    }
  }
#endif
  lvui::setHidden(v_->unreadView_.obj(), !list);
  return r;
}

void HomeScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const AppState& a = app();
  const lvui::Cell c = lvui::cellOf(v_->root_);
  const int rows = v_->text_.rows(), bottom = rows - 1;
#if RC_FEAT_UNREAD
  // The unread rows under the status lines, as the Unread screen lists them.
  if (homeShowsStatus()) {
    unreadItems_ = gatherUnread();
    std::vector<ListItem> items;
    items.reserve(unreadItems_.size());
    for (const UnreadItem& it : unreadItems_)
      items.push_back(ListItem{unreadLine(it),
                               it.kind == UnreadKind::Cmail ? utf8ToU32(std::to_string(it.count)) : U"",
                               ItemTone::Normal, true});
    unread_.setItems(std::move(items));
  }
#endif

  // Touch boards: the buttons (lvui::homeFit).
  lvui::HomeFit fit;
  const std::vector<lvui::HomeTileInfo> infos = board::hasTouch() ? tileInfos() : std::vector<lvui::HomeTileInfo>{};
  if (board::hasTouch()) {
    // The size the root's font is (ui_task's wantedSize; Home never asks for Compact).
    TextSize ts = a.cfg.textSize;
    if (TextSize only; board::fixedTextSize(only)) ts = only;
#if RC_LOW_MEM
    if (ts == TextSize::Compact) ts = TextSize::Comfortable;
#endif
    int labelCells = 0;
    for (const auto& i : infos) labelCells = std::max(labelCells, int(i.label.size()));
    // Without the Unread button, Messages spans the top row.
    const bool span = homeShowsStatus() && !tiles().empty() && tiles().front() == kMessages;
    fit = lvui::homeFit(int(lv_obj_get_width(v_->root_)), (rows - 2) * c.h, rows, ts, tileCount(), kTilePercent,
                        labelCells, span);
    column_ = fit.column;
    // A spanning button: arrows go by where the buttons lie (buttonMove).
    btnRects_.clear();
    if (std::find(fit.btns.side.begin(), fit.btns.side.end(), int8_t(0)) != fit.btns.side.end())
      btnRects_ = fit.btns.rects;
  }
  tileMode_ = !fit.btns.rects.empty() || !fit.lay.tiles.empty();
  lvui::setHidden(v_->tilesView_.obj(), !tileMode_);
  lvui::setHidden(v_->menuView_.obj(), tileMode_);
  const int panelW = int(lv_obj_get_width(v_->root_));
  // The page: rows 1 to the hint, the buttons and unread rows on it.
  lvui::place(v_->page_, 0, c.h, panelW, std::max(0, bottom - 1) * c.h);
  v_->text_.begin();
  if (tileMode_) {
    // The status lines between the button columns, or below the buttons
    // where the middle is too narrow.
    // Fluffy takes a middle too narrow for them. With neither there, the
    // buttons widen to meet in the middle (rc::widenHomeButtons).
    const bool aside = homeShowsStatus() && !fit.btns.rects.empty();
    if (!aside) rc::widenHomeButtons(fit.btns, panelW);
    if (aside) {
      const int bw = fit.btns.rects.back().w;  // a column's; the first may span
      const int left = (bw + rc::kHomeGapPx + c.w - 1) / c.w;
      const int right = (panelW - bw - rc::kHomeGapPx) / c.w;
      // The middle starts under a spanning button; the page goes on under the lowest.
      int top = 1, lowest = 0;
      for (size_t i = 0; i < fit.btns.rects.size(); ++i) {
        const rc::PxRect& r = fit.btns.rects[i];
        lowest = std::max(lowest, r.y + r.h);
        if (i < fit.btns.side.size() && fit.btns.side[i] == 0)
          top = std::max(top, (c.h + r.y + r.h + rc::kHomeGapPx + c.h - 1) / c.h);
      }
      const int below = (1 * c.h + lowest + rc::kHomeGapPx + c.h - 1) / c.h;
      const bool statusMid = right - left >= 24;
      const bool mascotMid = !statusMid && mascotFits(right - left, below - top);
      if (!statusMid && !mascotMid) rc::widenHomeButtons(fit.btns, panelW);
      // Laid out in full, drawn pageTop_ rows up; again if that was past the end.
      constexpr int kNoEnd = 1 << 16;
      for (int pass = 0; pass < 2; ++pass) {
        int end = below;
        if (statusMid) {
          end = std::max(end, syncAside(left, top, right - left, kNoEnd, pageTop_));
        } else {
          if (mascotMid)
            drawMascot(PageText{v_->text_, pageTop_, 1, bottom}, t, left, top, right - left, below - top);
          end = std::max(end, syncAside(1, below, v_->text_.cols() - 2, kNoEnd, pageTop_));
        }
        pageMax_ = std::max(0, end - bottom);
        if (pageTop_ <= pageMax_) break;
        pageTop_ = pageMax_;
        v_->text_.begin();
      }
    } else {
      pageTop_ = pageMax_ = 0;
      lvui::setHidden(v_->unreadView_.obj(), true);
    }
    v_->text_.end();
    if (!btnRects_.empty()) btnRects_ = fit.btns.rects;  // as widened
    syncStatus(v_->status_, a.user.empty() ? std::u32string(U"home") : utf8ToU32("@" + a.user), -1);
    int first = 0;
    if (column_) {
      colVisible_ = int(fit.lay.tiles.size());
      colStride_ = colVisible_ > 1 ? fit.lay.tiles[1].r0 - fit.lay.tiles[0].r0 : 3;
      colTop_ = std::max(0, std::min(colTop_, tileCount() - colVisible_));
      first = colTop_;
    }
    if (tileSel_ < 0 || tileSel_ >= tileCount()) tileSel_ = 0;
    // A page that scrolls takes drags anywhere on it.
    const lv_area_t bg = pageMax_ > 0 ? lv_area_t{0, 0, panelW - 1, (bottom - 1) * c.h - 1} : lv_area_t{0, 0, -1, -1};
    v_->tilesView_.syncHome(fit, -pageTop_ * c.h, infos, first, tileHidden_ ? -1 : tileSel_, board::eink(), bg);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
    return;
  }

  syncStatus(v_->status_, U"home", -1);
  // With the status lines, the fields and menu take the left and the lines
  // the right.
  const int cols = v_->text_.cols();
  const int leftW = homeShowsStatus() ? std::max(22, cols / 2) : cols - 1;
  int r = 1;
  for (const Field& f : listFields(t)) {
    v_->text_.line(1, r, f.label, t.dim);
    v_->text_.line(11, r++, f.value, f.fg, homeShowsStatus() ? leftW - 11 : -1);
  }
  pageTop_ = pageMax_ = 0;
  if (homeShowsStatus()) syncAside(leftW + 1, 1, cols - leftW - 2, bottom, 0);
  else lvui::setHidden(v_->unreadView_.obj(), true);
  v_->text_.end();
  std::vector<ListItem> items = menuRows();
  const int nItems = int(items.size());
  menu_.setItems(std::move(items));
  // A short grid scrolls the menu rather than draw it over the fields.
  placeCells(v_->menuView_.obj(), 1, std::max(r, bottom - nItems), leftW - 1, bottom);
  v_->menuView_.sync(menu_);
  syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open", U"tap to open");
}

}  // namespace rc
