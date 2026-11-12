// SPDX-License-Identifier: GPL-3.0-or-later
#include "feature_flags.h"
#if RC_FEAT_PROGRAMS

#include <algorithm>
#include <cstdlib>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "screens/lv_draw.h"
#include "screens/people_nav.h"
#include "screens/screens.h"
#include "store/locked_programs.h"
#include "ui/feed_layout.h"
#include "ui/wrap.h"

namespace rc {

namespace {

std::u32string age(int64_t ms) { return ms > 0 ? utf8ToU32(relativeAge(ms, nowEpochMs())) : std::u32string(); }

std::string kb(size_t bytes) { return std::to_string((bytes + 1023) / 1024) + " KB"; }

}  // namespace

// ---- the list ----

void ProgramListScreen::enter(const std::string&) {
  closeOverlays();
  failed_ = loading_ = false;
  const bool loaded = lockedPrograms().with([](ProgramsState& s) { return s.loaded; });
  if (!loaded) {
    list_.setItems({});
    list_.setSelected(0);
    request(false);
  }
}

void ProgramListScreen::request(bool more) {
  if (loading_) return;
  loading_ = true;
  failed_ = false;
  feedSource().command(Command{more ? CmdType::MorePrograms : CmdType::OpenPrograms});
}

void ProgramListScreen::onKey(const KeyEvent& k) {
  if (box_.isOpen()) return menuKey(k);
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      list_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right: {
      const int i = list_.selected();
      if (i < 0 || i >= int(ids_.size())) break;
      if (ids_[i].empty()) request(true);
      else router::go(router::Id::Program, ids_[i]);
      break;
    }
    case Action::Palette:
    case Action::Options:
      box_.open(U"programs", {ListItem{U"Refresh", U""}, ListItem{ui::textSizeItem(), U""}});
      break;
    case Action::Left:
    case Action::Back:
      feedSource().command(Command{CmdType::ClosePrograms});
      router::go(router::Id::Home);
      break;
    default:
      break;
  }
}

void ProgramListScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) return closeOverlays();
  if (box_.key(k) != Overlay::Result::Picked) return;
  if (box_.selected() == 0) {
    list_.setSelected(0);
    request(false);
  } else {
    ui::cycleTextSize();
  }
}

void ProgramListScreen::onEvent(const Event& e) {
  if (e.type != EvtType::ProgramsLoaded) return;
  loading_ = false;
  failed_ = !e.ok;
}

void ProgramListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  sync();
}

void ProgramListScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn), loading_ + 2 * failed_ + 4 * box_.isOpen()}))
    sync();
}

void ProgramListScreen::sync() {
  const Theme& t = lvui::currentTheme();
  std::vector<ListItem> items;
  ids_.clear();
  lockedPrograms().with([&](ProgramsState& s) {
    for (const auto& p : s.list) {
      items.push_back(ListItem{utf8ToU32(p.name.empty() ? p.id : p.name) + U"  @" + utf8ToU32(p.owner),
                               utf8ToU32(p.runtime)});
      ids_.push_back(p.id);
    }
    more_ = !s.cursor.empty();
  });
  if (more_ && !items.empty()) {
    items.push_back(ListItem{loading_ ? U"loading\u2026" : U"Older\u2026", U""});
    ids_.push_back("");
  }
  const bool empty = items.empty();
  const int cols = v_->text_.cols(), rows = v_->text_.rows(), bottom = rows - 1;
  const int nav = sideNavWidth(v_->text_);
  syncStatus(v_->status_, U"programs", -1);
  const int sel = list_.selected();
  list_.setItems(std::move(items));
  list_.setSelected(sel);
  placeCells(v_->listView_.obj(), 0, 1, cols - nav, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (empty) {
    const char* text = app().conn == 0 && (loading_ || failed_) ? "offline"
                       : failed_                                ? "couldn't load"
                       : loading_                               ? "loading\u2026"
                                                                : "no programs";
    v_->text_.line(1, 2, text, t.dim);
  }
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);
  if (box_.isOpen()) {
    const lvui::Cell c = v_->text_.cell();
    v_->menuView_.setPlace(OverlayPlace::Centre, rows * c.h, 0, 0);
    v_->menuView_.sync(box_);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else {
    lvui::setHidden(v_->menuView_.backdrop(), true);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap a program  \u2039 home");
  }
}

// ---- one program ----

void ProgramScreen::enter(const std::string& arg) {
  closeOverlays();
  const size_t at = arg.find('@');
  id_ = arg.substr(0, at);
  release_ = at == std::string::npos ? 0 : std::max(0, std::atoi(arg.c_str() + at + 1));
  latest_ = 0;
  lockedPrograms().with([&](ProgramsState& s) {
    for (const auto& p : s.list)
      if (p.id == id_) latest_ = p.release;
  });
  src_ = ProgramSource{};
  loaded_ = failed_ = loading_ = false;
  width_ = -1;
  text_.setText(U"");
  text_.setCode(wrap_);
  request();
}

void ProgramScreen::request() {
  if (loading_) return;
  loading_ = true;
  failed_ = false;
  Command c{CmdType::OpenProgram, id_};
  c.n = release_;
  feedSource().command(c);
}

void ProgramScreen::onKey(const KeyEvent& k) {
  if (box_.isOpen()) return menuKey(k);
  switch (k.action) {
    case Action::Up:
      text_.up();
      break;
    case Action::Down:
      text_.down();
      break;
    case Action::Right:
      text_.right();
      break;
    case Action::Left:
      if (text_.leftCol() > 0) {
        text_.left();
        break;
      }
      [[fallthrough]];
    case Action::Back:
      router::go(router::Id::Programs);
      break;
    case Action::Select:
    case Action::Palette:
    case Action::Options: {
      std::vector<ListItem> items;
      acts_.clear();
      if (latest_ > 1) {
        items.push_back(ListItem{U"Releases\u2026", U""});
        acts_.push_back(Act::Releases);
      }
      if (!src_.info.owner.empty()) {
        items.push_back(ListItem{U"Author's profile", U""});
        acts_.push_back(Act::Profile);
      }
      items.push_back(ListItem{wrap_ ? U"Don't wrap lines" : U"Wrap long lines", U""});
      acts_.push_back(Act::Wrap);
      items.push_back(ListItem{U"Refresh", U""});
      acts_.push_back(Act::Refresh);
      box_.open(U"program", std::move(items));
      break;
    }
    default:
      break;
  }
}

void ProgramScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) return closeOverlays();
  if (box_.key(k) != Overlay::Result::Picked) {
    if (!box_.isOpen()) releases_ = false;
    return;
  }
  const int i = box_.selected();
  if (releases_) {
    releases_ = false;
    const int n = latest_ - i;
    if (n >= 1) router::go(router::Id::Program, id_ + "@" + std::to_string(n));
    return;
  }
  if (i < 0 || i >= int(acts_.size())) return;
  switch (acts_[i]) {
    case Act::Releases: {
      std::vector<ListItem> items;
      const int shown = src_.info.release > 0 ? src_.info.release : latest_;
      for (int n = latest_; n >= 1; --n)
        items.push_back(ListItem{U"release " + utf8ToU32(std::to_string(n)), U"", ItemTone::Normal, n == shown});
      releases_ = true;
      box_.open(U"releases", std::move(items), std::max(0, latest_ - shown));
      break;
    }
    case Act::Profile: people::openProfile(src_.info.owner); break;
    case Act::Wrap:
      wrap_ = !wrap_;
      text_.setCode(wrap_);
      break;
    case Act::Refresh: request(); break;
  }
}
void ProgramScreen::onEvent(const Event& e) {
  if (e.type != EvtType::ProgramLoaded) return;
  if (e.text != id_ + "\n" + std::to_string(release_)) return;
  loading_ = false;
  if (!e.ok) {
    failed_ = !loaded_;
    return;
  }
  ProgramSource src;
  const bool mine = lockedPrograms().with([&](ProgramsState& s) {
    if (s.open.info.id != id_ || s.openRelease != release_) return false;
    src = s.open;
    return true;
  });
  if (!mine) return;
  src_ = std::move(src);
  latest_ = std::max(latest_, src_.info.release);
  loaded_ = true;
  failed_ = false;
  width_ = -1;  // refill
}

// The header wrapped to `width`, a rule, then the source (or why not).
void ProgramScreen::fill(int width) {
  const Theme& t = lvui::currentTheme();
  const ProgramInfo& p = src_.info;
  std::u32string text;
  std::vector<TextView::Style> styles;
  auto add = [&](std::u32string_view para, uint16_t fg, bool bold) {
    for (auto& l : wrapText(para, width, width)) {
      if (!styles.empty()) text += U'\n';
      text += l;
      styles.push_back(TextView::Style{fg, bold});
    }
  };
  add(utf8ToU32(p.name.empty() ? p.id : p.name), t.fg, true);
  std::string meta = "@" + p.owner + " \u00b7 " + p.runtime;
  if (p.release > 0) meta += " \u00b7 release " + std::to_string(p.release);
  std::u32string when = age(p.publishedMs);
  add(utf8ToU32(meta) + (when.empty() ? U"" : U" \u00b7 " + when), t.dim, false);
  if (!p.description.empty()) add(utf8ToU32(p.description), t.fg, false);
  add(std::u32string(size_t(std::max(1, width)), U'\u2500'), t.dim, false);
  if (src_.binary) {
    add(utf8ToU32("wasm binary, " + std::to_string(src_.bytes) + " bytes: can't be shown here"), t.dim, false);
  } else {
    text += U'\n';
    text += cleanSource(utf8ToU32(src_.text));
    if (src_.cut) {
      text += U'\n';
      text += utf8ToU32("\u2026 cut off at " + kb(kSourceShowMax));
    }
  }
  text_.setText(std::move(text), std::move(styles));
  width_ = width;
  colours_ = uint32_t(t.fg) << 16 | t.dim;
}

void ProgramScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->textView_.create(root);
  v_->cells_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  width_ = -1;
  sync();
}

void ProgramScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->cells_.obj(), nowMs, {int64_t(app().conn), loaded_ + 2 * failed_ + 4 * loading_}))
    sync();
}

void ProgramScreen::sync() {
  const Theme& t = lvui::currentTheme();
  std::u32string title = U"program";
  if (loaded_ && src_.info.release > 0) title += U" \u00b7 release " + utf8ToU32(std::to_string(src_.info.release));
  syncStatus(v_->status_, title, -1, loaded_);
  const int cols = v_->cells_.cols(), rows = v_->cells_.rows(), bottom = rows - 1;
  lvui::setHidden(v_->textView_.obj(), !loaded_);
  v_->cells_.begin();
  if (loaded_) {
    // One column short: the scroll bar's, when the source runs long.
    if (width_ != cols - 1 || colours_ != (uint32_t(t.fg) << 16 | t.dim)) fill(std::max(1, cols - 1));
    placeCells(v_->textView_.obj(), 0, 1, cols, bottom);
    v_->textView_.sync(text_);
  } else {
    const char* s = failed_ ? (app().conn == 0 ? "offline" : "couldn't load") : "loading\u2026";
    v_->cells_.line(1, 2, s, t.dim);
  }
  v_->cells_.end();
  if (box_.isOpen()) {
    const lvui::Cell c = v_->cells_.cell();
    v_->menuView_.setPlace(OverlayPlace::Centre, rows * c.h, 0, 0);
    v_->menuView_.sync(box_);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else {
    lvui::setHidden(v_->menuView_.backdrop(), true);
    syncHint(v_->hint_, U"\u2191\u2193\u2190\u2192 scroll  \u25CF menu  back: list",
             U"swipe to scroll  \u22ef menu  \u2039 list");
  }
}

}  // namespace rc

#endif  // RC_FEAT_PROGRAMS
