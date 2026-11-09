#include "feature_flags.h"
#if RC_FEAT_FEED

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "lvui/cell_font.h"
#include "screens/lv_draw.h"
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

void TopicsScreen::refresh(int listCols, const Theme& t, bool sameTheme) {
  if (dirty_ || listCols != cols_ || !sameTheme) {
    rebuild(listCols, t);
    theme_ = &t;
    dirty_ = false;
    cols_ = listCols;
  }
}

const char* TopicsScreen::emptyText() const {
  return waiting_ && app().conn == 0 ? "offline"
         : failed_ ? (board::hasTouch() ? "couldn't load: \u22ef, Refresh"
                                        : "couldn't load: Ctrl, Refresh")
         : waiting_ ? "loading\u2026"
                    : "no topics";
}

// ---- LVGL ----

void TopicsScreen::build(lv_obj_t* root) {
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

void TopicsScreen::update(int64_t nowMs) {
  // The topics change with TopicsUpdated, which ui::inputSeq() counts.
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn)})) sync();
}

void TopicsScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  const int listCols = cols - nav - EntryList::kBarCols;
  // The rows bake the theme's colours; lvui's theme is one object, so its
  // serial tells a change.
  const bool sameTheme = &t == theme_ && lvui::themeSerial() == themeSerial_;
  themeSerial_ = lvui::themeSerial();
  refresh(listCols, t, sameTheme);

  syncStatus(v_->status_, U"Topics", -1, true);
  const TextSize ts = app().cfg.textSize;
  const bool big = !palette_ && nav == 0;
  if (!big) list_.setBigHeads(0, 0, 0);
  else list_.setBigHeads(cellSize(ts).h, bigCellSize(ts).h, headCols());
  v_->listView_.setHeadFont(big ? lvui::font(ts, true) : nullptr);
  // The box's last column holds the scroll bar, which the outline stops short of.
  placeCells(v_->listView_.obj(), 0, 1, listCols + EntryList::kBarCols, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (list_.size() == 0) v_->text_.line(1, 2, withMenuKey(emptyText()), t.dim);
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);

  const lvui::Cell c = v_->text_.cell();
  v_->menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, nav * c.w, c.h);
  v_->menuView_.sync(box_);
  if (palette_)
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  else
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 feed  ctrl cmds", U"tap to open");
}

}  // namespace rc

#endif  // RC_FEAT_FEED
