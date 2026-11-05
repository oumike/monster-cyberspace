#include "feature_flags.h"
#if RC_FEAT_FEED

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

enum PaletteItem { kRefresh, kTextSize };

// Columns of a topic's first line, which is drawn one font size up.
int headCols() { return bigCols(board::panelWidth(), app().cfg.textSize); }

}  // namespace

void TopicsScreen::enter(const std::string&) {
  palette_ = false;
  dirty_ = true;
  // The list is fetched once per sign-in; Refresh fetches it again.
  bool loaded = lockedFeed().with([](FeedState& s) { return s.topicsLoaded; });
  if (!loaded) open();
}

void TopicsScreen::open() {
  failed_ = false;
  waiting_ = true;
  feedSource().command(Command{CmdType::OpenTopics});
}

void TopicsScreen::remember() {
  int i = list_.selected();
  if (i >= 0 && i < int(names_.size())) selName_ = names_[i];
}

void TopicsScreen::onKey(const KeyEvent& k) {
  if (palette_) return paletteKey(k);
  switch (k.action) {
    case Action::Up:
      list_.up();
      remember();
      break;
    case Action::Down:
      list_.down();
      remember();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) {
        remember();
        onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right: {
      int i = list_.selected();
      if (i < 0 || i >= int(names_.size())) break;
      router::go(router::Id::Topic, names_[i]);
      break;
    }
    case Action::Left:
    case Action::Back:
      router::go(router::Id::FeedList);
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

void TopicsScreen::paletteKey(const KeyEvent& k) {
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

void TopicsScreen::onEvent(const Event& e) {
  if (e.type != EvtType::TopicsUpdated) return;
  dirty_ = true;
  waiting_ = false;
  failed_ = !e.ok;
}

void TopicsScreen::rebuild(int cols, const Theme& t) {
  std::vector<Topic> topics;
  lockedFeed().with([&](FeedState& s) { topics = s.topics; });
  const int head = board::sideNav() ? cols : headCols() - EntryList::kBarCols;
  std::vector<FeedRow> rows;
  rows.reserve(topics.size());
  names_.clear();
  int sel = -1;
  for (const auto& tp : topics) {
    if (tp.name == selName_) sel = int(rows.size());
    rows.push_back(layoutTopicRow(tp, cols, t, head));
    names_.push_back(tp.name);
  }
  list_.setRows(std::move(rows));
  if (sel >= 0) list_.setSelected(sel);
  remember();
}

void TopicsScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  (void)nowMs;
  const int nav = sideNavWidth(g);
  const int listCols = g.cols() - nav - EntryList::kBarCols;
  if (dirty_ || listCols != cols_ || &t != theme_) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
  }

  g.fill(t.bg);
  drawStatus(g, t, U"Topics", -1, true);
  int bottom = g.rows() - 1;
  TextSize ts = app().cfg.textSize;
  if (palette_ || nav > 0) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  list_.render(g, 1, bottom, t, nav);
  if (list_.size() == 0) {
    const char* empty = waiting_ && app().conn == 0 ? "offline"
                        : failed_ ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                                       : "couldn't load: Ctrl, Refresh")
                        : waiting_ ? "loading\u2026"
                                   : "no topics";
    putLine(g, 1, 2, withMenuKey(empty), t.dim, t.bg);
  }

  if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
  if (palette_) {
    box_.render(g, 1, bottom, nav, OverlayPlace::BottomRight, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else {
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 feed  ctrl cmds", U"tap to open");
  }
}

}  // namespace rc

#endif  // RC_FEAT_FEED
