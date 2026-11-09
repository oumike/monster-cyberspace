#include "feature_flags.h"
#if RC_FEAT_GUILDS

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "lvui/cell_font.h"
#include "screens/lv_draw.h"
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
  // Without a first browse page there is no cursor to continue from.
  c.n = lockedFeed().with([](FeedState& s) { return s.guilds.browseLoaded(); }) ? 1 : 0;
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

void GuildListScreen::refresh(int listCols, const Theme& t, bool sameTheme) {
  if (dirty_ || listCols != cols_ || !sameTheme) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
  }
}

const char* GuildListScreen::emptyText(bool loading) const {
  bool waiting = loading || more_;
  return waiting && app().conn == 0 ? "offline"
         : failed_                  ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                                         : "couldn't load: Ctrl, Refresh")
         : waiting                  ? "loading\u2026"
                                    : "no guilds";
}

void GuildListScreen::hints(const char32_t*& keys, const char32_t*& touch) const {
  if (palette_) {
    keys = U"\u2191\u2193 pick  \u25CF choose  back: close";
    touch = U"tap: choose  outside: close";
  } else if (asked_) {
    keys = touch = U"loading more\u2026";
  } else {
    keys = U"\u2191\u2193 pick  \u25CF open  \u2190 home  ctrl cmds";
    touch = U"tap to open";
  }
}

// ---- LVGL ----

void GuildListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  themeSerial_ = ~0u;
  sync();
}

void GuildListScreen::update(int64_t nowMs) {
  // The list changes with GuildsUpdated and GuildUpdated, which
  // ui::inputSeq() counts; the store's loading flag (set with no event)
  // shows with the once-a-second refresh.
  if (gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn)})) sync();
}

void GuildListScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  const int listCols = cols - nav - EntryList::kBarCols;
  // The rows bake the theme's colours: lvui's theme is one object, so its
  // serial tells a change.
  const bool sameTheme = &t == theme_ && lvui::themeSerial() == themeSerial_;
  themeSerial_ = lvui::themeSerial();
  refresh(listCols, t, sameTheme);
  const bool loading = lockedFeed().with([](FeedState& s) { return s.loading; });

  syncStatus(v_->status_, U"Guilds", -1, true);
  const TextSize ts = app().cfg.textSize;
  const bool big = !palette_ && nav == 0;
  if (!big) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  v_->listView_.setHeadFont(big ? lvui::font(ts, true) : nullptr);
  // The box's last column holds the scroll bar, which the outline stops short of.
  placeCells(v_->listView_.obj(), 0, 1, listCols + EntryList::kBarCols, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (list_.size() == 0) v_->text_.line(1, 2, withMenuKey(emptyText(loading)), t.dim);
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);

  const lvui::Cell c = v_->text_.cell();
  v_->menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, nav * c.w, c.h);
  // The box shows only with palette_ (leave() keeps box_ open).
  if (palette_) v_->menuView_.sync(box_);
  else lvui::setHidden(v_->menuView_.backdrop(), true);
  const char32_t* keys;
  const char32_t* touch;
  hints(keys, touch);
  syncHint(v_->hint_, keys, touch);
}

}  // namespace rc

#endif  // RC_FEAT_GUILDS
