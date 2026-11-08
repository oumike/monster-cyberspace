#include "feature_flags.h"
#if RC_FEAT_PEOPLE

#include <algorithm>
#include <cctype>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "people/people_parse.h"
#include "screens/button_pad.h"
#include "screens/draw.h"
#include "screens/people_nav.h"
#include "screens/screens.h"
#include "store/locked_people.h"
#include "ui/nick_color.h"
#include "ui/button_grid.h"
#include "ui/profile_layout.h"
#include "ui/tap_map.h"

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

int ProfileScreen::linksTop(const Grid& g) const {
  const int bottom = g.rows() - 1;  // the last row holds the hint
  const int n = int(links_.size());
  if (!board::hasTouch()) return std::max(2, bottom - n);
  const int cols = linkColumns(g.cols(), links_);
  const int rows = (n + cols - 1) / cols;
  int top = std::max(2, bottom - (rows * kButtonRows + rows - 1));
  while (top > 2 && buttonGrid(g.cols(), top, bottom, n, cols, 0).maxScroll > 0) --top;
  return top;
}

void ProfileScreen::renderLinks(Grid& g, const Theme& t, int top, int bottom) {
  if (board::hasTouch()) {
    pad_.setCursor(linksFocus_);
    pad_.render(g, t, links_, top, bottom, false, linkColumns(g.cols(), links_));
    return;
  }
  for (int i = 0; i < int(links_.size()) && top + i < bottom; ++i) {
    const int row = top + i;
    g.fillRow(row, t.bg);
    g.putText(2, row, links_[i].label, t.sys, t.bg, g.cols() - 3);
    if (linksFocus_ && i == linkSel_ && cursorShown()) g.outline(0, row, g.cols() - 1, row, t.selectBg);
  }
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

void ProfileScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  if (dirty_ || nowMs - builtAt_ >= kAgeRefreshMs || styledFor_ != &t) {
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
  buildLinks();
  const bool actions = !actionsFor(snapshot(user_)).empty();
  g.fill(t.bg);
  drawStatus(g, t, U"@" + utf8ToU32(user_), -1, actions);
  if (linkOpen_) return link_.render(g, t);
  const int bottom = g.rows() - 1;
  // The links sit under the text, a blank row between them.
  const int linkTop = linksTop(g);
  renderLinks(g, t, linkTop, bottom);
  if (loaded_) {
    text_.render(g, 1, std::max(2, linkTop - 1), 0, g.cols(), t);
  } else {
    bool loading = lockedPeople().with([](PeopleState& s) { return s.loading; });
    const char* msg = missing_                        ? "no such user"
                      : failed_ && app().conn == 0     ? "offline"
                      : failed_                        ? "couldn't load: try Refresh"
                      : loading || app().conn != 0     ? "loading\u2026"
                                                       : "offline";
    putLine(g, 1, 2, msg, t.dim, t.bg);
  }
  if (menu_) {
    box_.render(g, 1, bottom, 0, OverlayPlace::BottomRight, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else if (busy_ || starting_) {
    drawHint(g, t, U"sending\u2026", U"sending\u2026");
  } else if (linksFocus_) {
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 back", U"tap to open  \u2039 back");
  } else if (actions) {
    drawHint(g, t, U"\u2191\u2193 scroll  \u25CF actions  \u2190 back",
             U"swipe to scroll  \u22ef actions  \u2039 back");
  } else {
    drawHint(g, t, U"\u2191\u2193 scroll  \u2190 back", U"swipe to scroll  \u2039 back");
  }
}

}  // namespace rc

#endif  // RC_FEAT_PEOPLE
