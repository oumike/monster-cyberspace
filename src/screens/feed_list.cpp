#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_feed.h"
#include "ui/feed_layout.h"

namespace rc {

namespace {

constexpr int64_t kAgeRefreshMs = 30000;  // re-lay rows so "3m" keeps up

enum PaletteItem { kNew, kRefresh, kWatched, kBookmarks, kTextSize };

bool compact() { return app().cfg.textSize == TextSize::Compact; }

// Columns of an entry's first line, which is drawn one font size up.
int headCols() { return bigCols(board::panelWidth(), app().cfg.textSize); }

}  // namespace

void FeedListScreen::enter(const std::string&) {
  palette_ = false;
  asked_ = false;
  dirty_ = true;
  // The feed shows what it holds at once; a side list waits for its own
  // list, since Watched and Bookmarks share the store's side list.
  loaded_ = kind_ == Kind::Feed;
  open();
}

void FeedListScreen::leave() {
  palette_ = false;
  if (kind_ == Kind::Feed) feedSource().command(Command{CmdType::CloseFeed});
}

void FeedListScreen::open() {
  failed_ = false;
  switch (kind_) {
    case Kind::Feed: feedSource().command(Command{CmdType::OpenFeed}); break;
    case Kind::Watched: feedSource().command(Command{CmdType::OpenWatched}); break;
    case Kind::Bookmarks: feedSource().command(Command{CmdType::OpenBookmarks}); break;
  }
}

void FeedListScreen::requestMore() {
  asked_ = true;
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
  if (palette_) return paletteKey(k);
  switch (k.action) {
    case Action::Up:
      list_.up();
      remember();
      break;
    case Action::Down:
      // The last row pages in more before wrapping; the selection stays put
      // until the page arrives.
      if (list_.atEnd() && more_) {
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
      if (i < 0 || i >= int(ids_.size())) break;
      // A bookmarked reply opens its entry with that reply selected.
      std::string reply = i < int(replyIds_.size()) ? replyIds_[i] : std::string();
      router::go(router::Id::Entry, reply.empty() ? ids_[i] : ids_[i] + "\n\n" + reply);
      break;
    }
    case Action::Left:
    case Action::Back:
      router::go(kind_ == Kind::Feed ? router::Id::Home : router::Id::FeedList);
      break;
    case Action::Palette:
    case Action::Options:
      palette_ = true;
      box_.open(U"commands", {
                                 ListItem{U"New entry", U""},
                                 ListItem{U"Refresh", U""},
                                 ListItem{U"Watched threads", U""},
                                 ListItem{U"Bookmarks", U""},
                                 ListItem{ui::textSizeItem(), U""},
                             });
      break;
    default:
      break;
  }
}

void FeedListScreen::paletteKey(const KeyEvent& k) {
  if (k.action == Action::Palette) {
    palette_ = false;
    box_.close();
    return;
  }
  switch (box_.key(k)) {
    case Overlay::Result::None:
      break;
    case Overlay::Result::Closed:
      palette_ = false;
      break;
    case Overlay::Result::Picked:
      palette_ = false;
      switch (box_.selected()) {
        case kNew:
          router::go(router::Id::Composer);
          break;
        case kRefresh:
          if (kind_ == Kind::Feed) feedSource().command(Command{CmdType::FeedRefresh});
          else open();
          break;
        case kWatched:
          router::go(router::Id::Watched);
          break;
        case kBookmarks:
          router::go(router::Id::Bookmarks);
          break;
        case kTextSize:
          ui::cycleTextSize();
          break;
      }
      break;
  }
}

void FeedListScreen::onEvent(const Event& e) {
  switch (e.type) {
    case EvtType::FeedUpdated:
      dirty_ = true;  // bookmark marks may have changed, for every kind
      if (kind_ == Kind::Feed) asked_ = false;
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
  // Copy out under the lock; lay out after it.
  lockedFeed().with([&](FeedState& s) {
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
  rows.reserve(entries.size());
  ids_.clear();
  replyIds_ = std::move(replyIds);
  for (size_t i = 0; i < entries.size(); ++i) {
    // With the side strip, the head lines stay in the plain font (FeedList render).
    rows.push_back(layoutFeedRow(entries[i], marked[i], now, cols, t, board::sideNav() ? cols : headCols() - EntryList::kBarCols));
    ids_.push_back(entries[i].id);
    if (side && sel < 0 && entries[i].id == selId_) sel = int(i);
  }
  list_.setRows(std::move(rows));
  if (sel >= 0) list_.setSelected(sel);
  remember();
}

void FeedListScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  // Big rows draw fewer, wider cells, which would push the side strip off the
  // panel; with it, every row stays plain.
  const int nav = sideNavWidth(g);
  const int listCols = g.cols() - nav - EntryList::kBarCols;
  if (dirty_ || listCols != cols_ || &t != theme_ || nowMs - builtAt_ >= kAgeRefreshMs) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
    builtAt_ = nowMs;
  }
  bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });

  g.fill(t.bg);
  const char32_t* title = kind_ == Kind::Feed      ? U"Feed \u21BB 2m"
                          : kind_ == Kind::Watched ? U"Watched"
                                                   : U"Bookmarks";
  drawStatus(g, t, title, -1, true);
  int bottom = g.rows() - 1;  // the last row holds the hint
  // The palette is drawn over the list in plain rows, so the list drops its
  // larger first lines while it's open.
  TextSize ts = app().cfg.textSize;
  if (palette_ || nav > 0) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  list_.render(g, 1, bottom, t, nav);
  if (list_.size() == 0) {
    // An empty feed with pages to come hasn't loaded yet.
    bool waiting = !loaded_ || loading || (kind_ == Kind::Feed && more_);
    const char* empty = waiting && app().conn == 0 ? "offline"
                        : !loaded_ && failed_    ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                                                      : "couldn't load: Ctrl, Refresh")
                        : waiting                ? "loading\u2026"
                        : kind_ == Kind::Feed    ? "no entries yet"
                        : kind_ == Kind::Watched ? "no watched threads"
                                                 : "no bookmarks";
    putLine(g, 1, 2, empty, t.dim, t.bg);
  }

  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (palette_) {
    box_.render(g, 1, bottom, nav, OverlayPlace::BottomRight, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else if (asked_) {
    drawHint(g, t, U"loading more\u2026", U"loading more\u2026");
  } else {
    drawHint(g, t,
             kind_ == Kind::Feed ? U"\u2191\u2193 pick  \u25CF open  \u2190 home  ctrl cmds"
                                 : U"\u2191\u2193 pick  \u25CF open  \u2190 feed  ctrl cmds",
             U"tap to open  hold: menu");
  }
}

}  // namespace rc
