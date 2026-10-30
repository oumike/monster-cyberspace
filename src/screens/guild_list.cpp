#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_feed.h"
#include "ui/guild_layout.h"

namespace rc {

namespace {

enum PaletteItem { kRefresh, kTextSize };

// Columns of a guild's first line, which is drawn one font size up.
int headCols() { return bigCols(board::panelWidth(), app().cfg.textSize); }

FeedRow divider(const char32_t* text, const Theme& t) { return FeedRow{text, U"", t.dim, t.dim}; }

}  // namespace

void GuildListScreen::enter(const std::string&) {
  palette_ = false;
  asked_ = false;
  dirty_ = true;
  selSlug_.clear();
  feedSource().command(Command{CmdType::CloseGuild});
  open();
}

void GuildListScreen::open() {
  failed_ = false;
  feedSource().command(Command{CmdType::OpenGuilds});
}

void GuildListScreen::requestMore() {
  asked_ = true;
  Command c{CmdType::OpenGuilds};
  c.n = 1;
  feedSource().command(c);
}

void GuildListScreen::remember() {
  int i = list_.selected();
  if (i >= 0 && i < int(slugs_.size())) selSlug_ = slugs_[i];
}

void GuildListScreen::step(bool down) {
  // Dividers are never selectable; the loop is bounded by the row count.
  for (size_t n = 0; n < slugs_.size(); ++n) {
    if (down) list_.down();
    else list_.up();
    int i = list_.selected();
    if (i >= 0 && i < int(slugs_.size()) && !slugs_[i].empty()) break;
  }
  remember();
}

void GuildListScreen::onKey(const KeyEvent& k) {
  if (palette_) return paletteKey(k);
  switch (k.action) {
    case Action::Up:
      step(false);
      break;
    case Action::Down:
      // The last row pages in more before wrapping.
      if (list_.atEnd() && more_) {
        if (!asked_) requestMore();
      } else {
        step(true);
      }
      break;
    case Action::Activate: {
      int i = k.index;
      if (k.target != uint8_t(ListId::Main) || i < 0 || i >= int(slugs_.size()) || slugs_[i].empty()) break;
      if (list_.select(i)) {
        if (list_.atEnd() && more_ && !asked_) requestMore();
        remember();
        onKey(KeyEvent{k.then});
      }
      break;
    }
    case Action::Select:
    case Action::Right: {
      int i = list_.selected();
      if (i < 0 || i >= int(slugs_.size()) || slugs_[i].empty()) break;
      router::go(router::Id::Guild, slugs_[i]);
      break;
    }
    case Action::Left:
    case Action::Back:
      router::go(router::Id::Home);
      break;
    case Action::Palette:
    case Action::Options:
      palette_ = true;
      box_.open(U"commands", {
                                 ListItem{U"Refresh", U""},
                                 ListItem{ui::textSizeItem(), U""},
                             });
      break;
    default:
      break;
  }
}

void GuildListScreen::paletteKey(const KeyEvent& k) {
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
        case kRefresh:
          open();
          break;
        case kTextSize:
          ui::cycleTextSize();
          break;
      }
      break;
  }
}

void GuildListScreen::onEvent(const Event& e) {
  if (e.type != EvtType::GuildsUpdated && e.type != EvtType::GuildUpdated) return;
  dirty_ = true;
  if (e.type != EvtType::GuildsUpdated) return;
  asked_ = false;
  failed_ = !e.ok;
}

void GuildListScreen::rebuild(int cols, const Theme& t) {
  std::vector<Guild> pinned, others;
  lockedFeed().with([&](FeedState& s) {
    pinned = s.guilds.pinned();
    others = s.guilds.others();
    more_ = !s.guilds.browseEnd();
  });
  const int head = board::sideNav() ? cols : headCols() - EntryList::kBarCols;
  std::vector<FeedRow> rows;
  slugs_.clear();
  auto add = [&](const std::vector<Guild>& gs) {
    for (const auto& g : gs) {
      rows.push_back(layoutGuildRow(g, cols, t, head));
      slugs_.push_back(g.slug);
    }
  };
  if (!pinned.empty()) {
    rows.push_back(divider(U"\u2500\u2500 my guilds \u2500\u2500", t));
    slugs_.emplace_back();
    add(pinned);
  }
  if (!others.empty()) {
    rows.push_back(divider(U"\u2500\u2500 all guilds \u2500\u2500", t));
    slugs_.emplace_back();
    add(others);
  }
  int sel = -1;
  for (size_t i = 0; i < slugs_.size(); ++i) {
    if (slugs_[i].empty()) continue;
    if (sel < 0) sel = int(i);
    if (slugs_[i] == selSlug_) {
      sel = int(i);
      break;
    }
  }
  list_.setRows(std::move(rows));
  if (sel >= 0) list_.setSelected(sel);
  remember();
}

void GuildListScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  (void)nowMs;
  const int nav = sideNavWidth(g);
  const int listCols = g.cols() - nav - EntryList::kBarCols;
  if (dirty_ || listCols != cols_ || &t != theme_) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
  }
  bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });

  g.fill(t.bg);
  drawStatus(g, t, U"Guilds", -1, true);
  int bottom = g.rows() - 1;
  TextSize ts = app().cfg.textSize;
  if (palette_ || nav > 0) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  list_.render(g, 1, bottom, t, nav);
  if (list_.size() == 0) {
    bool waiting = loading || more_;
    const char* empty = waiting && app().conn == 0 ? "offline"
                        : failed_                  ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                                                        : "couldn't load: Ctrl, Refresh")
                        : waiting                  ? "loading\u2026"
                                                   : "no guilds";
    putLine(g, 1, 2, empty, t.dim, t.bg);
  }

  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (palette_) {
    box_.render(g, 1, bottom, nav, OverlayPlace::BottomRight, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else if (asked_) {
    drawHint(g, t, U"loading more\u2026", U"loading more\u2026");
  } else {
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home  ctrl cmds", U"tap to open");
  }
}

}  // namespace rc
