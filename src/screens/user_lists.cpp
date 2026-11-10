#include "feature_flags.h"
#if RC_FEAT_PEOPLE

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "feed/markdown_text.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/locked_people.h"
#include "ui/feed_layout.h"
#include "ui/nick_color.h"

namespace rc {

namespace {

// The reply's first non-empty line, as plain text.
std::u32string firstLine(const std::string& md) {
  std::u32string t = markdownToText(md);
  size_t start = 0;
  while (start < t.size()) {
    size_t nl = t.find(U'\n', start);
    if (nl == std::u32string::npos) nl = t.size();
    if (nl > start) return t.substr(start, nl - start);
    start = nl + 1;
  }
  return U"";
}

}  // namespace

void UserListScreen::enter(const std::string& username) {
  user_ = username;
  failed_ = false;
  list_.setItems({});
  list_.setSelected(0);
  request(false);
}

void UserListScreen::request(bool append) {
  asked_ = append;
  loading_ = true;
  Command c{guilds_ ? CmdType::OpenUserGuilds : CmdType::OpenUserReplies};
  c.a = user_;
  c.n = append ? 1 : 0;
  feedSource().command(c);
}

void UserListScreen::onKey(const KeyEvent& k) {
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
      int i = list_.selected();
      if (i < 0 || i >= int(keys_.size()) || keys_[i].empty()) break;
      if (!guilds_) {
#if RC_FEAT_ENTRIES
        router::go(router::Id::Entry, keys_[i]);
#endif
      } else {
#if RC_FEAT_GUILDS
        router::goFrom(router::Id::UserGuilds, router::Id::Guild, keys_[i]);
#endif
      }
      break;
    }
    case Action::Left:
    case Action::Back:
      router::go(router::Id::Profile, user_);
      break;
    default:
      break;
  }
}

void UserListScreen::onEvent(const Event& e) {
  if (e.type != (guilds_ ? EvtType::UserGuildsUpdated : EvtType::UserRepliesUpdated)) return;
  asked_ = loading_ = false;
  failed_ = !e.ok;
}

std::vector<ListItem> UserListScreen::rows(int64_t nowMs) {
  std::vector<UserReply> replies;
  std::vector<UserGuild> guilds;
  lockedPeople().with([&](PeopleState& s) {
    if (!sameUsername(s.listUser, user_)) return;
    if (guilds_) {
      guilds = s.guilds;
      more_ = false;
    } else {
      replies = s.replies;
      more_ = !s.repliesCursor.empty();
    }
  });
  std::vector<ListItem> items;
  keys_.clear();
  if (guilds_) {
    for (const auto& g : guilds) {
      ListItem it;
      std::string name = g.name.empty() ? g.slug : g.name;
      it.label = utf8ToU32(g.icon.empty() ? name : g.icon + " " + name);
      it.right = utf8ToU32(g.role);
      items.push_back(std::move(it));
      keys_.push_back(g.slug);
    }
  } else {
    for (const auto& r : replies) {
      ListItem it;
      it.label = firstLine(r.content);
      if (r.createdMs >= 0) it.right = utf8ToU32(relativeAge(r.createdMs, nowMs));
      items.push_back(std::move(it));
      keys_.push_back(r.postId.empty() ? std::string() : r.postId + "\n\n" + r.replyId);
    }
  }
  return items;
}

const char* UserListScreen::emptyText() const {
  return loading_ && app().conn == 0 ? "offline"
         : failed_                   ? "couldn't load"
         : loading_                  ? "loading\u2026"
         : guilds_                   ? "not in a guild"
                                     : "no replies yet";
}

// ---- LVGL ----

void UserListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync(nowMs_);
}

void UserListScreen::update(int64_t nowMs) {
  nowMs_ = nowMs;
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn)})) sync(nowMs);
}

void UserListScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  std::vector<ListItem> items = rows(nowMs);
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  syncStatus(v_->status_, (guilds_ ? U"guilds \u00b7 @" : U"replies \u00b7 @") + utf8ToU32(user_), -1);
  const bool empty = items.empty();
  list_.setItems(std::move(items));
  placeCells(v_->listView_.obj(), 0, 1, cols - nav, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (empty) v_->text_.line(1, 2, emptyText(), t.dim);
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);
  if (asked_) {
    syncHint(v_->hint_, U"loading more\u2026", U"loading more\u2026");
  } else if (guilds_) {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF guild  \u2190 profile", U"tap a guild  \u2039 profile");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 profile", U"tap a reply  \u2039 profile");
  }
}

}  // namespace rc

#endif  // RC_FEAT_PEOPLE
