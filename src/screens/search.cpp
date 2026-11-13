// SPDX-License-Identifier: GPL-3.0-or-later
#include "feature_flags.h"
#if RC_FEAT_SEARCH

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "screens/lv_draw.h"
#include "screens/people_nav.h"
#include "screens/screens.h"
#include "store/locked_search.h"
#include "ui/feed_layout.h"

namespace rc {

namespace {

constexpr size_t kGroupMax = 8;  // type=all's cap per group

// A row's key: "u<username>", "e<postId>", "r<postId>\n\n<replyId>" or
// "m<type>" (More…); "" for a heading.
std::string keyOf(const SearchHit& h) {
  switch (h.kind) {
    case SearchKind::User: return "u" + h.user;
    case SearchKind::Post: return "e" + h.id;
    case SearchKind::Reply: return "r" + h.postId + "\n\n" + h.id;
  }
  return {};
}

ListItem itemOf(const SearchHit& h) {
  ListItem it;
  if (h.kind == SearchKind::User) {
    it.label = utf8ToU32("@" + h.user + (h.title.empty() ? "" : "  " + h.title));
    return it;
  }
  it.label = utf8ToU32(h.kind == SearchKind::Post && !h.title.empty() ? h.title : h.line);
  std::string right = "@" + h.user;
  if (h.createdMs > 0) right += " \u00b7 " + relativeAge(h.createdMs, nowEpochMs());
  it.right = utf8ToU32(right);
  return it;
}

// Opens a row's key from `from`; false for a heading or nothing.
bool openKey(router::Id from, const std::string& key) {
  if (key.empty()) return false;
  const std::string rest = key.substr(1);
  switch (key[0]) {
    case 'u':
      SearchScreen::markReturn();
      people::openProfile(rest);
      return true;
    case 'e':
    case 'r':
      SearchScreen::markReturn();
      router::goFrom(from, router::Id::Entry, rest);
      return true;
    case 'm':
      SearchScreen::markReturn();
      router::go(router::Id::SearchList, rest);
      return true;
  }
  return false;
}

std::u32string groupName(const std::string& type) {
  return type == "users" ? U"people" : type == "replies" ? U"replies" : U"entries";
}

}  // namespace

bool SearchScreen::returning_ = false;

// ---- Search ----

void SearchScreen::enter(const std::string&) {
  busy_ = failed_ = false;
  const bool back = returning_;
  returning_ = false;
  if (back && lockedSearch().with([](SearchState& s) { return s.searched; })) {
    editing_ = false;  // the list as it was
    return;
  }
  editing_ = true;
  input_.setText(utf8ToU32(query_));
  list_.setItems({});
  list_.setSelected(0);
}

void SearchScreen::run() {
  const std::string q = searchQuery(input_.utf8());
  if (q.empty()) return ui::banner("Type something to search for", true);
  if (!searchQueryOk(q)) return ui::banner("Search is too long", true);
  if (app().conn == 0) return ui::banner("Offline: not searched", true);
  query_ = q;
  busy_ = true;
  failed_ = false;
  feedSource().command(Command{CmdType::Search, q});
}

bool SearchScreen::step(bool down) {
  const int n = int(keys_.size());
  for (int i = list_.selected() + (down ? 1 : -1); i >= 0 && i < n; i += down ? 1 : -1) {
    if (!keys_[i].empty()) {
      list_.setSelected(i);
      return true;
    }
  }
  return false;
}

bool SearchScreen::selectFirst() {
  // The list must hold the rows before a selection past its old end sticks.
  list_.setItems(rows(nowMs_));
  for (size_t i = 0; i < keys_.size(); ++i) {
    if (!keys_[i].empty()) {
      list_.setSelected(int(i));
      return true;
    }
  }
  return false;
}

void SearchScreen::onKey(const KeyEvent& k) {
  if (editing_) return editKey(k);
  switch (k.action) {
    case Action::Up:
      if (!step(false)) editing_ = true;
      break;
    case Action::Down:
      step(true);
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) {
        const int i = list_.selected();
        if (i >= 0 && i < int(keys_.size()) && !keys_[i].empty()) onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right: {
      const int i = list_.selected();
      if (i >= 0 && i < int(keys_.size())) openKey(router::Id::Search, keys_[i]);
      break;
    }
    case Action::Char:
    case Action::Focus:
      editing_ = true;
      editKey(k);
      break;
    case Action::Left:
    case Action::Back:
      feedSource().command(Command{CmdType::CloseSearch});
      router::go(router::Id::Home);
      break;
    default:
      break;
  }
}

void SearchScreen::editKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Char:
      if (k.ch != U'\n') input_.insert(k.ch);
      break;
    case Action::Backspace:
      input_.backspace();
      break;
    case Action::ClearLine:
      input_.clear();
      break;
    case Action::Left:
      input_.left();
      break;
    case Action::Right:
      input_.right();
      break;
    case Action::Focus:
      input_.setCursor(k.index);
      break;
    case Action::Select:
      if (!busy_) run();
      break;
    case Action::Down:
      if (selectFirst()) editing_ = false;
      break;
    case Action::Activate:
      // A tapped hit leaves the query line for the list.
      if (k.target == uint8_t(ListId::Main)) {
        editing_ = false;
        onKey(k);
      }
      break;
    case Action::Back:
      if (!keys_.empty() && selectFirst()) {
        editing_ = false;
      } else {
        feedSource().command(Command{CmdType::CloseSearch});
        router::go(router::Id::Home);
      }
      break;
    default:
      break;
  }
}

void SearchScreen::onEvent(const Event& e) {
  if (e.type != EvtType::SearchUpdated || e.text != "all" || !busy_) return;
  busy_ = false;
  failed_ = !e.ok;
  // Hits: down to the first. None: keep typing.
  if (e.ok && selectFirst()) editing_ = false;
}

std::vector<ListItem> SearchScreen::rows(int64_t) {
  std::vector<ListItem> items;
  keys_.clear();
  lockedSearch().with([&](SearchState& s) {
    if (!s.searched) return;
    auto group = [&](const char32_t* heading, const std::vector<SearchHit>& hits, const char* type) {
      if (hits.empty()) return;
      items.push_back(ListItem{heading, U"", ItemTone::Dim});
      keys_.emplace_back();
      for (const auto& h : hits) {
        items.push_back(itemOf(h));
        keys_.push_back(keyOf(h));
      }
      if (hits.size() >= kGroupMax) {
        items.push_back(ListItem{U"More\u2026", U""});
        keys_.push_back(std::string("m") + type);
      }
    };
    group(U"People", s.users, "users");
    group(U"Entries", s.posts, "posts");
    group(U"Replies", s.replies, "replies");
  });
  return items;
}

void SearchScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->inputView_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync(nowMs_);
}

void SearchScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(editing_) | int64_t(busy_) << 1})) sync(nowMs);
}

void SearchScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  std::vector<ListItem> items = rows(nowMs);
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  syncStatus(v_->status_, U"search", -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  input_.setDisabled(busy_ || !editing_);
  placeCells(v_->inputView_.obj(), 0, 1, cols, 2);
  v_->inputView_.sync(input_, U"search> ");
  placeCells(v_->listView_.obj(), 0, 2, cols - nav, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (empty) {
    const char* msg = busy_ && app().conn == 0 ? "offline"
                      : busy_                  ? "searching\u2026"
                      : failed_                ? "couldn't search"
                      : lockedSearch().with([](SearchState& s) { return s.searched; })
                          ? "nothing found"
                          : "people, entries and replies";
    v_->text_.line(1, 3, msg, t.dim);
  }
  v_->text_.end();
  v_->navView_.sync(nav, 2, bottom, cols);
  if (busy_) {
    syncHint(v_->hint_, U"searching\u2026", U"searching\u2026");
  } else if (editing_) {
    syncHint(v_->hint_, U"type  \u25CF search  \u2193 results  back: home", U"type  enter: search");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  \u2039 home");
  }
}

// ---- one group, paged ----

void SearchListScreen::enter(const std::string& type) {
  failed_ = false;
  const bool same = type == type_ && lockedSearch().with([&](SearchState& s) {
    return s.listType == type && s.listLoaded;
  });
  type_ = type;
  if (same) return;  // back from a hit: the list as it was
  list_.setItems({});
  list_.setSelected(0);
  request(false);
}

void SearchListScreen::request(bool next) {
  asked_ = next;
  loading_ = true;
  Command c{CmdType::SearchMore, type_};
  c.n = next ? 1 : 0;
  feedSource().command(c);
}

void SearchListScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      // The last row pages in more before wrapping.
      if (list_.selected() == int(list_.size()) - 1 && more_) {
        if (!asked_) request(true);
      } else {
        list_.down();
      }
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) {
        if (list_.selected() == int(list_.size()) - 1 && more_ && !asked_) request(true);
        onKey(KeyEvent{k.then});
      }
      break;
    case Action::Select:
    case Action::Right: {
      const int i = list_.selected();
      if (i >= 0 && i < int(keys_.size())) openKey(router::Id::SearchList, keys_[i]);
      break;
    }
    case Action::Left:
    case Action::Back:
      SearchScreen::markReturn();
      router::go(router::Id::Search);
      break;
    default:
      break;
  }
}

void SearchListScreen::onEvent(const Event& e) {
  if (e.type != EvtType::SearchUpdated || e.text != type_) return;
  asked_ = loading_ = false;
  failed_ = !e.ok;
}

std::vector<ListItem> SearchListScreen::rows(int64_t) {
  std::vector<ListItem> items;
  keys_.clear();
  lockedSearch().with([&](SearchState& s) {
    if (s.listType != type_) return;
    more_ = !s.cursor.empty();
    for (const auto& h : s.list) {
      items.push_back(itemOf(h));
      keys_.push_back(keyOf(h));
    }
  });
  return items;
}

void SearchListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync(nowMs_);
}

void SearchListScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn)})) sync(nowMs);
}

void SearchListScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  std::vector<ListItem> items = rows(nowMs);
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  const std::string q = lockedSearch().with([](SearchState& s) { return s.query; });
  syncStatus(v_->status_, groupName(type_) + U" \u00b7 " + utf8ToU32(q), -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  placeCells(v_->listView_.obj(), 0, 1, cols - nav, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (empty) {
    const char* msg = loading_ && app().conn == 0 ? "offline"
                      : failed_                   ? "couldn't load"
                      : loading_                  ? "loading\u2026"
                                                  : "nothing found";
    v_->text_.line(1, 2, msg, t.dim);
  }
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);
  if (asked_) {
    syncHint(v_->hint_, U"loading more\u2026", U"loading more\u2026");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 search", U"tap to open  \u2039 search");
  }
}

}  // namespace rc

#endif  // RC_FEAT_SEARCH
