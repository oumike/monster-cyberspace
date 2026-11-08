#include "feature_flags.h"
#if RC_FEAT_ENTRIES

#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "lvui/cell_font.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_feed.h"
#include "ui/feed_layout.h"
#include "ui/nick_color.h"

namespace rc {

namespace {

constexpr int64_t kAgeRefreshMs = 30000;  // re-lay rows so "3m" keeps up

enum PaletteItem {
  kNew, kRefresh, kWatched, kBookmarks, kTopics, kTextSize,
  kInfo, kMembers, kJoin, kLeave, kMakeMine, kProfile, kOlder,
};

bool compact() { return app().cfg.textSize == TextSize::Compact; }

// Columns of an entry's first line, which is drawn one font size up.
int headCols() { return bigCols(board::panelWidth(), app().cfg.textSize); }

}  // namespace

void FeedListScreen::enter(const std::string& arg) {
  palette_ = confirm_ = false;
  box_.close();
  info_.close();
  if (kind_ == Kind::Guild || kind_ == Kind::User || kind_ == Kind::Topic) slug_ = arg;
  asked_ = false;
  dirty_ = true;
  // The feed shows what it holds at once; a side list waits for its own
  // list, since Watched and Bookmarks share the store's side list.
  loaded_ = kind_ == Kind::Feed;
  // Following: back from an entry, the list it already holds (it scans several
  // Feed pages, too many to repeat); Refresh reloads it.
  if (kind_ == Kind::Following) {
    if (lockedFeed().with([](FeedState& s) { return s.store.sideTag() == kFollowingSideTag; })) {
      loaded_ = true;
      return;
    }
  }
  open();
}

void FeedListScreen::leave() {
  palette_ = confirm_ = false;
  box_.close();
  info_.close();
  if (kind_ == Kind::Feed) feedSource().command(Command{CmdType::CloseFeed});
}

void FeedListScreen::open() {
  failed_ = false;
  switch (kind_) {
    case Kind::Feed: feedSource().command(Command{CmdType::OpenFeed}); break;
    case Kind::Watched: feedSource().command(Command{CmdType::OpenWatched}); break;
    case Kind::Bookmarks: feedSource().command(Command{CmdType::OpenBookmarks}); break;
    case Kind::Guild: feedSource().command(Command{CmdType::OpenGuild, slug_}); break;
    case Kind::User: feedSource().command(Command{CmdType::OpenUserPosts, slug_}); break;
    case Kind::Topic: feedSource().command(Command{CmdType::OpenTopic, slug_}); break;
    case Kind::Following: feedSource().command(Command{CmdType::OpenFollowingPosts}); break;
  }
}

void FeedListScreen::requestMore() {
  asked_ = true;
  if (kind_ == Kind::Guild) return feedSource().command(Command{CmdType::GuildThreadsMore, slug_});
  if (kind_ == Kind::Following) {
    Command c{CmdType::OpenFollowingPosts};
    c.n = 1;
    return feedSource().command(c);
  }
  if (kind_ == Kind::User || kind_ == Kind::Topic) {
    Command c{kind_ == Kind::User ? CmdType::OpenUserPosts : CmdType::OpenTopic, slug_};
    c.n = 1;
    return feedSource().command(c);
  }
  Command c{kind_ == Kind::Feed        ? CmdType::FeedMore
            : kind_ == Kind::Watched   ? CmdType::OpenWatched
                                       : CmdType::OpenBookmarks};
  if (kind_ != Kind::Feed) c.n = 1;
  feedSource().command(c);
}

void FeedListScreen::remember() {
  int i = list_.selected();
  if (i >= 0 && i < int(ids_.size())) selId_ = ids_[i];
}

void FeedListScreen::onKey(const KeyEvent& k) {
  if (info_.isOpen()) {
    info_.key(k);
    return;
  }
  if (palette_) return paletteKey(k);
  switch (k.action) {
    case Action::Up:
      list_.up();
      remember();
      break;
    case Action::Down:
      // The last row pages in more before wrapping; the selection stays put
      // until the page arrives.
      // An empty side list with more to come pages in too (Following).
      if ((list_.atEnd() || (empty_ && kind_ != Kind::Feed)) && more_) {
        if (!asked_) requestMore();
      } else {
        list_.down();
      }
      remember();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) {
        // A drag can land on the last row directly, so page more here too.
        if (list_.atEnd() && more_ && !asked_) requestMore();
        remember();
        onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right: {
      int i = list_.selected();
      // The Feed's New entry row and a guild's New thread row: its id is
      // empty, and it composes, behind the same check as the commands menu's
      // New entry / New thread (boards that can't type write from web config).
      if (composeRow_ && i == 0) {
        if (kind_ == Kind::Guild) {
          if (!ui::typingElsewhere("Start threads")) router::go(router::Id::Composer, "guild:" + slug_);
        } else if (!ui::typingElsewhere("Write entries")) {
          router::go(router::Id::Composer);
        }
        break;
      }
      if (i < 0 || i >= int(ids_.size())) break;
      // A bookmarked reply opens its entry with that reply selected.
      std::string reply = i < int(replyIds_.size()) ? replyIds_[i] : std::string();
      router::go(router::Id::Entry, reply.empty() ? ids_[i] : ids_[i] + "\n\n" + reply);
      break;
    }
    case Action::Left:
    case Action::Back:
      if (kind_ == Kind::User) {
        router::go(router::Id::Profile, slug_);
        break;
      }
      if (kind_ == Kind::Bookmarks && router::backToOrigin()) break;
      if (kind_ == Kind::Topic) {
        router::go(router::Id::Topics);
        break;
      }
      router::go(kind_ == Kind::Feed || kind_ == Kind::Following ? router::Id::Home
                 : kind_ == Kind::Guild ? router::Id::GuildList
                                        : router::Id::FeedList);
      break;
    case Action::Palette:
    case Action::Options:
      openPalette();
      break;
    default:
      break;
  }
}

void FeedListScreen::openPalette() {
  palette_ = true;
  confirm_ = false;
  acts_.clear();
  std::vector<ListItem> items;
  auto add = [&](int act, const char32_t* label) {
    acts_.push_back(act);
    items.push_back(ListItem{label, U""});
  };
  if (kind_ == Kind::Guild) {
    // Founders manage membership on the web, so they get no membership items.
    std::string role;
    bool have = lockedFeed().with([&](FeedState& s) {
      const Guild* g = s.guilds.guild();
      if (g) role = g->role;
      return g != nullptr;
    });
    if (have) {
      add(kNew, U"New thread");
      add(kInfo, U"Info");
      add(kMembers, U"Members");
      if (role.empty()) add(kJoin, U"Join");
      if (role == "apprentice") add(kMakeMine, U"Make my guild");
      if (role == "apprentice" || role == "member") add(kLeave, U"Leave");
    }
    add(kRefresh, U"Refresh");
    acts_.push_back(kTextSize);
    items.push_back(ListItem{ui::textSizeItem(), U""});
  } else if (kind_ == Kind::User || kind_ == Kind::Topic || kind_ == Kind::Following) {
    if (kind_ == Kind::User) add(kProfile, U"Profile");
    // Following finds few entries a scan; touch has no Down to page on.
    if (kind_ == Kind::Following && more_ && !asked_) add(kOlder, U"Load older");
    add(kRefresh, U"Refresh");
    acts_.push_back(kTextSize);
    items.push_back(ListItem{ui::textSizeItem(), U""});
  } else {
    add(kNew, U"New entry");
    add(kRefresh, U"Refresh");
    add(kWatched, U"Watched threads");
    add(kBookmarks, U"Bookmarks");
    add(kTopics, U"Topics");
    acts_.push_back(kTextSize);
    items.push_back(ListItem{ui::textSizeItem(), U""});
  }
  box_.open(U"commands", std::move(items));
}

void FeedListScreen::askConfirm(CmdType type, const Guild& g) {
  std::string name = g.name.empty() ? g.slug : g.name;
  std::string text;
  std::u32string action;
  if (type == CmdType::JoinGuild) {
    text = "Join " + name + "? (3 per minute, 15 per day)";
    action = U"Join";
  } else if (type == CmdType::LeaveGuild) {
    text = "Leave " + name + "? (3 per minute, 15 per day)";
    if (g.role == "member") text += " Your badge will be cleared.";
    action = U"Leave";
  } else {
    text = "Make " + name + " my guild? (3 per minute, 15 per day)";
    action = U"Make my guild";
  }
  pending_ = type;
  confirm_ = palette_ = true;
  box_.confirm(utf8ToU32(text), action);
}

void FeedListScreen::paletteKey(const KeyEvent& k) {
  if (k.action == Action::Palette) {
    palette_ = confirm_ = false;
    box_.close();
    return;
  }
  switch (box_.key(k)) {
    case Overlay::Result::None:
      break;
    case Overlay::Result::Closed:
      palette_ = confirm_ = false;
      break;
    case Overlay::Result::Picked: {
      palette_ = false;
      if (confirm_) {
        confirm_ = false;
        if (box_.selected() != 1) break;
        if (app().conn == 0) {
          ui::banner("Offline: not sent", true);
          break;
        }
        feedSource().command(Command{pending_, slug_});
        break;
      }
      int sel = box_.selected();
      if (sel < 0 || sel >= int(acts_.size())) break;
      switch (acts_[sel]) {
        case kNew:
          if (ui::typingElsewhere(kind_ == Kind::Guild ? "Start threads" : "Write entries")) break;
          if (kind_ == Kind::Guild) router::go(router::Id::Composer, "guild:" + slug_);
          else router::go(router::Id::Composer);
          break;
        case kInfo:
        case kJoin:
        case kLeave:
        case kMakeMine: {
          Guild g;
          bool have = lockedFeed().with([&](FeedState& s) {
            const Guild* cur = s.guilds.guild();
            if (cur) g = *cur;
            return cur != nullptr;
          });
          if (!have) break;
          if (acts_[sel] == kInfo) info_.open(g);
          else askConfirm(acts_[sel] == kJoin ? CmdType::JoinGuild
                          : acts_[sel] == kLeave ? CmdType::LeaveGuild
                                                 : CmdType::PromoteGuild, g);
          break;
        }
        case kMembers:
          router::go(router::Id::Members, slug_);
          break;
        case kProfile:
          router::go(router::Id::Profile, slug_);
          break;
        case kOlder:
          if (!asked_) requestMore();
          break;
        case kRefresh:
          if (kind_ == Kind::Guild) {
            loaded_ = false;
            open();
          } else if (kind_ == Kind::Feed) feedSource().command(Command{CmdType::FeedRefresh});
          else open();
          break;
        case kWatched:
          router::go(router::Id::Watched);
          break;
        case kBookmarks:
          router::go(router::Id::Bookmarks);
          break;
        case kTopics:
          router::go(router::Id::Topics);
          break;
        case kTextSize:
          ui::cycleTextSize();
          break;
      }
      break;
    }
  }
}

void FeedListScreen::onEvent(const Event& e) {
  switch (e.type) {
    case EvtType::FeedUpdated:
      dirty_ = true;  // bookmark marks may have changed, for every kind
      if (kind_ == Kind::Feed) asked_ = false;
      break;
    case EvtType::GuildUpdated:
      if (kind_ != Kind::Guild) break;
      dirty_ = true;
      if (!e.ok && e.code == 404) router::go(router::Id::GuildList);
      else if (!e.ok && !loaded_) failed_ = true;
      break;
    case EvtType::GuildWriteDone:
      if (kind_ != Kind::Guild) break;
      ui::banner(e.text, !e.ok);
      if (!e.ok && e.code == 404) router::go(router::Id::GuildList);
      break;
    case EvtType::SideListUpdated:
      if (kind_ == Kind::Feed) break;
      dirty_ = true;
      asked_ = false;
      // A failed load leaves the other list (they share the store) hidden.
      if (e.ok) loaded_ = true;
      else if (!loaded_) failed_ = true;
      break;
    case EvtType::EntryPosted:
      // The store doesn't insert a posted entry; the first page brings it.
      if (kind_ == Kind::Feed && e.ok) feedSource().command(Command{CmdType::FeedRefresh});
      break;
    default:
      break;
  }
}

void FeedListScreen::rebuild(int cols, const Theme& t) {
  std::vector<Entry> entries;
  std::vector<std::string> replyIds;
  std::vector<bool> marked;
  int sel = -1;
  bool side = kind_ != Kind::Feed;
  bool haveGuild = false;
  // Copy out under the lock; lay out after it.
  lockedFeed().with([&](FeedState& s) {
    haveGuild = s.guilds.guild() != nullptr;
    more_ = side ? !s.store.sideCursor().empty() : !s.store.feedEnd();
    if (!loaded_) return;
    entries = side ? s.store.sideList() : s.store.feed();
    if (kind_ == Kind::Bookmarks) replyIds = s.store.sideReplyIds();
    marked.reserve(entries.size());
    for (const auto& e : entries) marked.push_back(!s.store.bookmarkIdFor(e.id).empty());
    if (!selId_.empty() && !side) sel = s.store.indexOf(selId_);
  });
  if (!loaded_) more_ = false;
  int64_t now = nowEpochMs();
  std::vector<FeedRow> rows;
  rows.reserve(entries.size() + 1);
  ids_.clear();
  replyIds_ = std::move(replyIds);
  // The Feed's first row composes: one Select opens the Composer, with no menu
  // in the way, so a d-pad and Select alone can write a post. A guild's does
  // too (a new thread), once the guild is known, as its palette's New thread.
  const bool guild = kind_ == Kind::Guild;
  const bool composeRow = kind_ == Kind::Feed || (guild && haveGuild);
  composeRow_ = composeRow;
  if (composeRow) {
    const int headW = std::max(1, headCols() - EntryList::kBarCols);
    std::u32string head = utf8ToU32(guild ? "+ New thread" : "+ New entry");
    if (int(head.size()) > headW) head.resize(headW);
    std::u32string detail = utf8ToU32(
        guild ? (board::hasTouch() ? "tap to start a thread" : "press \u25CF to start a thread")
              : (board::hasTouch() ? "tap to write a post" : "press \u25CF to write a post"));
    if (int(detail.size()) > cols) detail.resize(cols);
    rows.push_back(FeedRow{head, detail, t.fg, t.dim});
    ids_.push_back(std::string());
  }
  empty_ = entries.empty();
  for (size_t i = 0; i < entries.size(); ++i) {
    // With the side strip, the head lines stay in the plain font (FeedList render).
    rows.push_back(layoutFeedRow(entries[i], marked[i], now, cols, t, board::sideNav() ? cols : headCols() - EntryList::kBarCols));
    ids_.push_back(entries[i].id);
    if (side && sel < 0 && entries[i].id == selId_) sel = int(i);
  }
  list_.setRows(std::move(rows));
  if (sel >= 0) list_.setSelected(sel + (composeRow ? 1 : 0));
  remember();
}

std::u32string FeedListScreen::guildTitle() const {
  std::string t = lockedFeed().with([](FeedState& s) {
    const Guild* g = s.guilds.guild();
    if (!g) return std::string();
    std::string n = g->name.empty() ? g->slug : g->name;
    return g->icon.empty() ? n : g->icon + " " + n;
  });
  return t.empty() ? std::u32string(U"Guild") : utf8ToU32(t);
}

void FeedListScreen::refresh(int listCols, const Theme& t, int64_t nowMs, bool sameTheme) {
  if (dirty_ || listCols != cols_ || !sameTheme || nowMs - builtAt_ >= kAgeRefreshMs) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
    builtAt_ = nowMs;
  }
}

std::u32string FeedListScreen::title() const {
  if (kind_ == Kind::Guild) return guildTitle();
  if (kind_ == Kind::User) return U"entries \u00b7 @" + utf8ToU32(slug_);
  if (kind_ == Kind::Topic) return U"#" + utf8ToU32(slug_);
  return kind_ == Kind::Feed        ? U"Feed \u21BB 2m"
         : kind_ == Kind::Watched   ? U"Watched"
         : kind_ == Kind::Following ? U"Following"
                                    : U"Bookmarks";
}

const char* FeedListScreen::emptyText(bool loading) const {
  // An empty feed with pages to come hasn't loaded yet.
  bool waiting = !loaded_ || loading || (kind_ == Kind::Feed && more_);
  const bool self = kind_ == Kind::User && !app().user.empty() && sameUsername(slug_, app().user);
  return waiting && app().conn == 0 ? "offline"
         : !loaded_ && failed_    ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                                       : "couldn't load: Ctrl, Refresh")
         : waiting                ? "loading\u2026"
         : kind_ == Kind::Feed    ? "no entries yet"
         : kind_ == Kind::Watched ? "no watched threads"
         : kind_ == Kind::Guild   ? "no threads yet"
         : kind_ == Kind::User    ? (self ? "no entries yet" : "no public entries")
         : kind_ == Kind::Topic   ? "no entries"
         : kind_ == Kind::Following ? (more_ ? (board::hasTouch() ? "none in recent entries: \u22ef, Load older"
                                                                 : "none in recent entries: \u2193 for older")
                                             : "nothing from people you follow")
                                  : "no bookmarks";
}

void FeedListScreen::hints(const char32_t*& keys, const char32_t*& touch) const {
  if (palette_) {
    keys = U"\u2191\u2193 pick  \u25CF choose  back: close";
    touch = U"tap: choose  outside: close";
  } else if (asked_) {
    keys = touch = U"loading more\u2026";
  } else {
    keys = kind_ == Kind::Feed || kind_ == Kind::Following
                                  ? U"\u2191\u2193 pick  \u25CF open  \u2190 home  ctrl cmds"
           : kind_ == Kind::Guild ? U"\u2191\u2193 pick  \u25CF open  \u2190 guilds  ctrl cmds"
           : kind_ == Kind::User  ? U"\u2191\u2193 pick  \u25CF open  \u2190 profile  ctrl cmds"
           : kind_ == Kind::Topic ? U"\u2191\u2193 pick  \u25CF open  \u2190 topics  ctrl cmds"
                                  : U"\u2191\u2193 pick  \u25CF open  \u2190 feed  ctrl cmds";
    touch = U"tap to open  hold: menu";
  }
}

void FeedListScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  // Big rows draw fewer, wider cells, which would push the side strip off the
  // panel; with it, every row stays plain.
  const int nav = sideNavWidth(g);
  const int listCols = g.cols() - nav - EntryList::kBarCols;
  refresh(listCols, t, nowMs, &t == theme_);
  bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });

  g.fill(t.bg);
  if (info_.isOpen()) {
    drawStatus(g, t, guildTitle(), -1);
    info_.render(g, t);
    return;
  }
  drawStatus(g, t, title(), -1, true);
  int bottom = g.rows() - 1;  // the last row holds the hint
  // The palette is drawn over the list in plain rows, so the list drops its
  // larger first lines while it's open.
  TextSize ts = app().cfg.textSize;
  if (palette_ || nav > 0) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  list_.render(g, 1, bottom, t, nav);
  // Below the New entry / New thread row, which takes the first two.
  if (empty_) putLine(g, 1, composeRow_ ? 3 : 2, withMenuKey(emptyText(loading)), t.dim, t.bg);

  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (palette_) box_.render(g, 1, bottom, nav, OverlayPlace::BottomRight, t);
  const char32_t* keys;
  const char32_t* touch;
  hints(keys, touch);
  drawHint(g, t, keys, touch);
}

// ---- LVGL ----

void FeedListScreen::build(lv_obj_t* root) {
  listView_.create(root);
  navView_.create(root);
  text_.create(root);
  infoText_.create(root);
  infoQr_.create(root);
  hint_.create(root);
  status_.create(root);
  menuView_.create(root);
  themeSerial_ = ~0u;
  sync(nowMs_);
}

void FeedListScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  // The lists change with FeedUpdated, SideListUpdated and GuildUpdated,
  // which ui::inputSeq() counts; the store's loading flag (set with no event)
  // and the ages show with the once-a-second refresh.
  if (gate_.due(text_.obj(), nowMs, {int64_t(app().conn)})) sync(nowMs);
}

void FeedListScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  const int cols = text_.cols(), bottom = text_.rows() - 1;
  const int nav = sideNavWidth(text_);
  const int listCols = cols - nav - EntryList::kBarCols;
  // The rows bake the theme's colours: lvui's theme is one object, so its
  // serial tells a change.
  const bool sameTheme = &t == theme_ && lvui::themeSerial() == themeSerial_;
  themeSerial_ = lvui::themeSerial();
  refresh(listCols, t, nowMs, sameTheme);
  const bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });

  const bool info = info_.isOpen();
  lvui::setHidden(listView_.obj(), info);
  text_.begin();
  if (info) {
    syncStatus(status_, guildTitle(), -1);
    navView_.sync(0, 1, bottom, cols);
    info_.sync(infoText_, text_, infoQr_, hint_);
    text_.end();
    menuView_.sync(box_);
    return;
  }
  lvui::setHidden(infoText_.obj(), true);
  lvui::setHidden(infoQr_.obj(), true);
  syncStatus(status_, title(), -1, true);
  const TextSize ts = app().cfg.textSize;
  const bool big = !palette_ && nav == 0;
  if (!big) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  listView_.setHeadFont(big ? lvui::font(ts, true) : nullptr);
  // The box's last column holds the scroll bar, which the outline stops short of.
  placeCells(listView_.obj(), 0, 1, listCols + EntryList::kBarCols, bottom);
  listView_.sync(list_);
  if (empty_) text_.line(1, composeRow_ ? 3 : 2, withMenuKey(emptyText(loading)), t.dim);
  text_.end();
  navView_.sync(nav, 1, bottom, cols);

  const lvui::Cell c = text_.cell();
  menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, nav * c.w, c.h);
  menuView_.sync(box_);
  const char32_t* keys;
  const char32_t* touch;
  hints(keys, touch);
  syncHint(hint_, keys, touch);
}

}  // namespace rc

#endif  // RC_FEAT_ENTRIES
