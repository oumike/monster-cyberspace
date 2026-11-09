#include "feature_flags.h"
#if RC_FEAT_PEOPLE

#include <algorithm>
#include <cctype>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "lvui/styles.h"
#include "people/people_parse.h"
#include "screens/button_pad.h"
#include "screens/lv_draw.h"
#include "screens/people_nav.h"
#include "screens/screens.h"
#include "store/locked_people.h"
#include "ui/nick_color.h"
#include "ui/button_grid.h"
#include "ui/profile_layout.h"

namespace rc {

namespace {

constexpr int64_t kAgeRefreshMs = 60000;  // re-lay so "active 3m ago" keeps up

enum ProfileAct {
  kFollow, kUnfollow, kMessage, kPoke, kEntries, kBookmarks, kGuild, kWebsite, kRefresh
};

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool isSelf(const std::string& username) {
  return !app().user.empty() && lower(username) == lower(app().user);
}

// The open profile, when it's the one asked for.
struct View {
  Profile p;
  bool loaded = false, loading = false, blocked = false;
  std::string followId;
};

View snapshot(const std::string& user) {
  View v;
  lockedPeople().with([&](PeopleState& s) {
    v.loading = s.loading;
    if (!s.loaded || lower(s.profile.username) != lower(user)) return;
    v.loaded = true;
    v.p = s.profile;
    v.blocked = hasUsername(s.blocked, s.profile.username);
    v.followId = s.follows.followIdFor(s.profile.userId);
  });
  return v;
}

// The actions menu: what you can do to someone else. Entries, Bookmarks (your
// own profile), Guild, Website and Refresh are links under the text instead (ProfileScreen::buildLinks).
std::vector<std::pair<int, const char32_t*>> actionsFor(const View& v) {
  std::vector<std::pair<int, const char32_t*>> out;
  if (!v.loaded || isSelf(v.p.username)) return out;
  if (!v.followId.empty()) out.emplace_back(kUnfollow, U"Unfollow");
  else if (!v.blocked && !v.p.userId.empty()) out.emplace_back(kFollow, U"Follow");
  // C-Mail and pokes are refused (403) when either side blocked the other.
  if (!v.blocked) out.emplace_back(kMessage, U"Message");
  if (!v.blocked) out.emplace_back(kPoke, U"Poke");
  return out;
}

// Link buttons side by side when every label fits, else fewer to a row.
int linkColumns(int gridCols, const std::vector<ListItem>& items) {
  size_t longest = 0;
  for (const auto& it : items) longest = std::max(longest, it.label.size());
  return buttonColumnsFor(gridCols, int(items.size()), int(longest));
}

}  // namespace

void ProfileScreen::enter(const std::string& username) {
  closeOverlays();
  // Back from Entries or a Guild re-enters the same profile: keep it and
  // its scroll, and don't fetch it again.
  bool same = !user_.empty() && lower(user_) == lower(username) && loaded_;
  user_ = username;
  busy_ = starting_ = false;
  linksFocus_ = false;
  pad_.reset();
  if (same) return;
  linkSel_ = 0;
  loaded_ = failed_ = missing_ = false;
  dirty_ = true;
  shown_.clear();
  text_.setText(U"");
  feedSource().command(Command{CmdType::OpenProfile, user_});
}

void ProfileScreen::onKey(const KeyEvent& k) {
  if (linkOpen_) {
    if (k.action == Action::Back || k.action == Action::Select || k.action == Action::Left ||
        k.action == Action::Activate)
      linkOpen_ = false;
    return;
  }
  if (menu_) return menuKey(k);
  const bool touch = board::hasTouch();
  const bool fromTouch = ui::fromTouch();
  const int n = int(linkActs_.size());
  // A tap on a link button.
  if (touch && k.action == Action::Activate && k.target == uint8_t(ListId::Main)) {
    linksFocus_ = false;
    pad_.setCursor(false);
    const int i = pad_.key(k);
    if (i >= 0 && i < n) doAct(linkActs_[i]);
    return;
  }
  // Drags scroll the text, and leave the links.
  if (fromTouch && (k.action == Action::Up || k.action == Action::Down)) {
    linksFocus_ = false;
    if (k.action == Action::Up) text_.up();
    else text_.down();
    return;
  }
  if (linksFocus_ && n > 0) {
    switch (k.action) {
      case Action::Up:
        if (touch ? pad_.atTopEdge() : linkSel_ == 0) linksFocus_ = false;
        else if (touch) pad_.key(k);
        else --linkSel_;
        return;
      case Action::Down:
        if (touch) pad_.key(k);
        else linkSel_ = std::min(linkSel_ + 1, n - 1);
        return;
      case Action::Right:
        if (touch) {
          pad_.key(k);
          return;
        }
        break;  // the list: Right opens, as Select
      case Action::Left:
        if (touch && !pad_.atLeftEdge()) {
          pad_.key(k);
          return;
        }
        people::back();
        return;
      default:
        break;
    }
    if (k.action == Action::Select || k.action == Action::Right) {
      const int i = touch ? pad_.key(KeyEvent{Action::Select}) : linkSel_;
      if (i >= 0 && i < n) doAct(linkActs_[i]);
      return;
    }
  }
  const bool actions = !actionsFor(snapshot(user_)).empty();
  // Into the links: Down at the end of the text, or Select with no actions.
  auto focusLinks = [&] {
    if (n == 0 || !board::hasKeys()) return false;
    linksFocus_ = true;
    linkSel_ = 0;
    pad_.showCursorAt(0);
    return true;
  };
  switch (k.action) {
    case Action::Up:
      text_.up();
      break;
    case Action::Down:
      if (!text_.atEnd() || !focusLinks()) text_.down();
      break;
    case Action::Select:
    case Action::Right:
      if (actions) openMenu();
      else focusLinks();
      break;
    case Action::Palette:
    case Action::Options:
      if (actions) openMenu();
      break;
    case Action::Left:
    case Action::Back:
      people::back();
      break;
    default:
      break;
  }
}

void ProfileScreen::buildLinks() {
  View v = snapshot(user_);
  linkActs_.clear();
  links_.clear();
  auto add = [&](int act, const char32_t* label) {
    linkActs_.push_back(act);
    links_.push_back(ListItem{label, U""});
  };
  if (v.loaded) {
    add(kEntries, U"Entries");
    if (isSelf(v.p.username)) add(kBookmarks, U"Bookmarks");
    if (!v.p.guildSlug.empty()) add(kGuild, U"Guild");
    if (!v.p.websiteUrl.empty()) add(kWebsite, U"Website");
  }
  add(kRefresh, U"Refresh");
  linkSel_ = std::clamp(linkSel_, 0, int(links_.size()) - 1);
}

int ProfileScreen::linksTop(int gridRows, int gridCols) const {
  const int bottom = gridRows - 1;  // the last row holds the hint
  const int n = int(links_.size());
  if (!board::hasTouch()) return std::max(2, bottom - n);
  const int cols = linkColumns(gridCols, links_);
  const int rows = (n + cols - 1) / cols;
  int top = std::max(2, bottom - (rows * kButtonRows + rows - 1));
  while (top > 2 && buttonGrid(gridCols, top, bottom, n, cols, 0).maxScroll > 0) --top;
  return top;
}

void ProfileScreen::openMenu() {
  View v = snapshot(user_);
  acts_.clear();
  std::vector<ListItem> items;
  auto add = [&](int act, const char32_t* label) {
    acts_.push_back(act);
    items.push_back(ListItem{label, U""});
  };
  for (const auto& a : actionsFor(v)) add(a.first, a.second);
  if (items.empty()) return;
  menu_ = true;
  confirm_ = false;
  box_.open(U"@" + utf8ToU32(user_), std::move(items));
}

void ProfileScreen::ask(int act) {
  std::string text;
  std::u32string action;
  if (act == kFollow) {
    text = "Follow @" + user_ + "? (3 per minute, 15 per day)";
    action = U"Follow";
  } else if (act == kUnfollow) {
    text = "Unfollow @" + user_ + "? (3 per minute, 15 per day)";
    action = U"Unfollow";
  } else {
    text = "Poke @" + user_ + "? (1 an hour, 8 a day)";
    action = U"Poke";
  }
  pending_ = act;
  menu_ = confirm_ = true;
  box_.confirm(utf8ToU32(text), action);
}

void ProfileScreen::send(int act) {
  if (busy_) {
    ui::banner("Busy", true);
    return;
  }
  if (app().conn == 0) {
    ui::banner("Offline: not sent", true);
    return;
  }
  View v = snapshot(user_);
  if (!v.loaded) return;
  busy_ = true;
  if (act == kPoke) {
    feedSource().command(Command{CmdType::Poke, v.p.username});
    return;
  }
  feedSource().command(
      Command{act == kFollow ? CmdType::Follow : CmdType::Unfollow, v.p.userId, v.p.username});
}

void ProfileScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) {
    closeOverlays();
    return;
  }
  switch (box_.key(k)) {
    case Overlay::Result::None:
      break;
    case Overlay::Result::Closed:
      menu_ = confirm_ = false;
      break;
    case Overlay::Result::Picked: {
      menu_ = false;
      if (confirm_) {
        confirm_ = false;
        if (box_.selected() == 1) send(pending_);
        break;
      }
      int sel = box_.selected();
      if (sel < 0 || sel >= int(acts_.size())) break;
      doAct(acts_[sel]);
      break;
    }
  }
}

void ProfileScreen::doAct(int act) {
  View v = snapshot(user_);
  switch (act) {
    case kFollow:
    case kUnfollow:
    case kPoke:
      ask(act);
      break;
    case kMessage:
      if (starting_) break;
      if (app().conn == 0) {
        ui::banner("Offline: not sent", true);
        break;
      }
      starting_ = true;
      chatSource().command(Command{CmdType::StartConversation, user_});
      break;
    case kEntries:
      router::go(router::Id::UserPosts, user_);
      break;
    case kBookmarks:
      router::goFrom(router::Id::Profile, router::Id::Bookmarks);
      break;
    case kGuild:
      if (!v.p.guildSlug.empty()) router::go(router::Id::Guild, v.p.guildSlug);
      break;
    case kWebsite:
      if (!v.p.websiteUrl.empty()) {
        link_.open(v.p.websiteUrl);
        linkOpen_ = true;
      }
      break;
    case kRefresh:
      loaded_ = failed_ = missing_ = false;
      dirty_ = true;
      feedSource().command(Command{CmdType::OpenProfile, user_});
      break;
  }
}

void ProfileScreen::onEvent(const Event& e) {
  switch (e.type) {
    case EvtType::ProfileUpdated:
      dirty_ = true;
      if (e.ok) {
        failed_ = missing_ = false;
      } else if (!loaded_) {
        failed_ = true;
        missing_ = e.code == 404;
      }
      break;
    case EvtType::PeopleWriteDone:
      if (!busy_) break;
      busy_ = false;
      ui::banner(e.text, !e.ok);
      dirty_ = true;
      break;
    case EvtType::ConversationStarted:
      if (!starting_) break;
      starting_ = false;
      if (e.ok) router::go(router::Id::Conversation, e.text);
      else ui::banner(e.text.empty() ? std::string("Couldn't start a conversation") : e.text, true);
      break;
    default:
      break;
  }
}

void ProfileScreen::refreshText(const Theme& t, int64_t nowMs, bool sameTheme) {
  if (dirty_ || nowMs - builtAt_ >= kAgeRefreshMs || !sameTheme) {
    View v = snapshot(user_);
    if (v.loaded) {
      loaded_ = true;
      const bool self = sameUsername(v.p.username, app().user);
      const uint16_t nick = nickColor(t, v.p.username, self);
      std::u32string text;
      std::vector<TextView::Style> styles;
      for (const auto& l : profileLines(v.p, v.blocked, nowEpochMs(), !v.followId.empty())) {
        TextView::Style st{t.fg};
        switch (l.role) {
          case ProfileRole::Name: st = {nick, true}; break;
          case ProfileRole::Handle: st = {nick}; break;
          case ProfileRole::Guild: st = {t.sys, true}; break;
          case ProfileRole::Meta: st = {t.dim}; break;
          case ProfileRole::Active: st = {t.time}; break;
          case ProfileRole::Counts: st = {t.fg}; break;
          case ProfileRole::Website: st = {t.sys}; break;
          case ProfileRole::Supporter: st = {t.selfNick, true}; break;
          case ProfileRole::Following: st = {t.sys}; break;
          case ProfileRole::Blocked: st = {t.error, true}; break;
          case ProfileRole::Bio: st = {t.fg}; break;
        }
        // One style per paragraph: the bio may hold several.
        size_t paras = 1;
        for (char32_t c : l.text) paras += c == U'\n';
        styles.insert(styles.end(), paras, st);
        if (!text.empty() || styles.size() > paras) text += U'\n';
        text += l.text;
      }
      // Keep the scroll: only a changed text goes back to the top.
      shown_ = text;
      text_.setText(std::move(text), std::move(styles));
    }
    dirty_ = false;
    builtAt_ = nowMs;
    styledFor_ = &t;
  }
}

const char* ProfileScreen::emptyText(bool loading) const {
  return missing_                        ? "no such user"
         : failed_ && app().conn == 0     ? "offline"
         : failed_                        ? "couldn't load: try Refresh"
         : loading || app().conn != 0     ? "loading\u2026"
                                          : "offline";
}

void ProfileScreen::hints(bool actions, const char32_t*& keys, const char32_t*& touch) const {
  if (menu_) {
    keys = U"\u2191\u2193 pick  \u25CF choose  back: close";
    touch = U"tap: choose  outside: close";
  } else if (busy_ || starting_) {
    keys = touch = U"sending\u2026";
  } else if (linksFocus_) {
    keys = U"\u2191\u2193 pick  \u25CF open  \u2190 back";
    touch = U"tap to open  \u2039 back";
  } else if (actions) {
    keys = U"\u2191\u2193 scroll  \u25CF actions  \u2190 back";
    touch = U"swipe to scroll  \u22ef actions  \u2039 back";
  } else {
    keys = U"\u2191\u2193 scroll  \u2190 back";
    touch = U"swipe to scroll  \u2039 back";
  }
}

// ---- LVGL ----

void ProfileScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->textView_.create(root);
  v_->padView_.create(root);
  linkFocus_ = lvui::plainBox(root);
  lvui::addStyle(linkFocus_, lvui::Role::Focused);
  v_->cells_.create(root);
  v_->qrView_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  themeSerial_ = ~0u;
  sync(nowMs_);
}

void ProfileScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  // The profile, follows and blocks change with ProfileUpdated and
  // PeopleWriteDone, which ui::inputSeq() counts; the store's loading flag
  // (set with no event) and the ages show with the once-a-second refresh.
  if (gate_.due(v_->cells_.obj(), nowMs, {int64_t(app().conn)})) sync(nowMs);
}

void ProfileScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  // The text bakes the theme's colours: lvui's theme is one object, so its
  // serial tells a change.
  const bool sameTheme = styledFor_ == &t && lvui::themeSerial() == themeSerial_;
  themeSerial_ = lvui::themeSerial();
  refreshText(t, nowMs, sameTheme);
  buildLinks();
  const bool actions = !actionsFor(snapshot(user_)).empty();
  syncStatus(v_->status_, U"@" + utf8ToU32(user_), -1, actions);
  const int cols = v_->cells_.cols(), rows = v_->cells_.rows(), bottom = rows - 1;
  const bool touch = board::hasTouch();
  bool outline = false;
  v_->cells_.begin();
  lvui::setHidden(v_->textView_.obj(), linkOpen_ || !loaded_);
  lvui::setHidden(v_->padView_.obj(), linkOpen_ || !touch);
  if (linkOpen_) {
    link_.sync(v_->cells_, v_->qrView_, v_->hint_);
    v_->cells_.end();
    lvui::setHidden(linkFocus_, true);
    lvui::setHidden(v_->menuView_.backdrop(), true);
    return;
  }
  lvui::setHidden(v_->qrView_.obj(), true);
  // The links sit under the text, a blank row between them.
  const int linkTop = linksTop(rows, cols);
  if (touch) {
    pad_.setCursor(linksFocus_);
    pad_.sync(v_->padView_, links_, linkTop, bottom, false, linkColumns(cols, links_));
  } else {
    for (int i = 0; i < int(links_.size()) && linkTop + i < bottom; ++i) {
      const int row = linkTop + i;
      v_->cells_.line(2, row, links_[i].label, t.sys, cols - 3);
      if (linksFocus_ && i == linkSel_ && cursorShown()) {
        placeCells(linkFocus_, 0, row, cols, row + 1);
        outline = true;
      }
    }
  }
  lvui::setHidden(linkFocus_, !outline);
  if (loaded_) {
    placeCells(v_->textView_.obj(), 0, 1, cols, std::max(2, linkTop - 1));
    v_->textView_.sync(text_);
  } else {
    const bool loading = lockedPeople().with([](PeopleState& s) { return s.loading; });
    v_->cells_.line(1, 2, emptyText(loading), t.dim);
  }
  v_->cells_.end();
  if (menu_) {
    const lvui::Cell c = v_->cells_.cell();
    v_->menuView_.setPlace(OverlayPlace::BottomRight, bottom * c.h, 0, c.h);
    v_->menuView_.sync(box_);
  } else {
    lvui::setHidden(v_->menuView_.backdrop(), true);
  }
  const char32_t* keys;
  const char32_t* touchHint;
  hints(actions, keys, touchHint);
  syncHint(v_->hint_, keys, touchHint);
}

}  // namespace rc

#endif  // RC_FEAT_PEOPLE
