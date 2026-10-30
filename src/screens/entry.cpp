#include "screens/entry.h"

#include <algorithm>
#include <cctype>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/compose.h"
#include "feed/feed_source.h"
#include "screens/draw.h"
#include "store/locked_feed.h"
#include "ui/feed_layout.h"

namespace rc {

namespace {

constexpr int64_t kAgeRefreshMs = 60000;  // re-lay so "3m" keeps up
constexpr size_t kReasonMaxChars = 500;
// Pages of a block past the first are tagged "<id>\x1f<n>" so the log can
// step through a long body or reply a screen at a time.
constexpr char kPageSep = '\x1f';

// The entry or reply id behind a (possibly paged) log id.
std::string baseId(const std::string& id) {
  size_t p = id.find(kPageSep);
  return p == std::string::npos ? id : id.substr(0, p);
}

bool compact() { return app().cfg.textSize == TextSize::Compact; }

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool isSelf(const std::string& author) {
  return !app().user.empty() && lower(author) == lower(app().user);
}

std::string entryUrl(const Entry& e) {
  return "https://cyberspace.online/" + e.author + "/" + e.slug;
}

}  // namespace

void EntryScreen::enter(const std::string& arg) {
  spell_.hide();
  std::string parts[3];
  size_t start = 0;
  for (int i = 0; i < 3; ++i) {
    size_t nl = i < 2 ? arg.find('\n', start) : std::string::npos;
    parts[i] = arg.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  postId_ = parts[0];
  noticeId_ = parts[1];
  wantReply_ = parts[2];
  repliesArrived_ = false;
  log_.setLines({});
  log_.follow();
  input_.clear();
  view_ = View::None;
  reasonMode_ = false;
  loaded_ = hidden_ = revealed_ = sending_ = moreAsked_ = false;
  title_.clear();
  authors_.clear();
  dirty_ = true;
  feedSource().command(Command{CmdType::OpenEntry, postId_, noticeId_});
}

void EntryScreen::leave() {
  view_ = View::None;
  box_.close();
  reasonMode_ = false;
  sending_ = false;
  log_.setLines({});
  authors_.clear();
  lockedFeed().with([](FeedState& s) { s.store.closeEntry(); });
}

bool EntryScreen::hasOverlay() const {
  return view_ != View::None || reasonMode_ || !log_.following();
}

void EntryScreen::closeOverlays() {
  view_ = View::None;
  box_.close();
  if (reasonMode_) {
    reasonMode_ = false;
    input_.setText(savedInput_);
  }
  log_.follow();
}

void EntryScreen::leaveToList() {
  const router::Place base = router::lastBase();
  if (base.id == router::Id::Guild) router::go(router::Id::Guild, base.arg);
  else router::go(router::Id::FeedList);
}

std::string EntryScreen::selectedReply() const {
  std::string id = baseId(log_.selectedId());
  return id == postId_ ? std::string() : id;
}

// ---- keys ----

void EntryScreen::onKey(const KeyEvent& k) {
  if (view_ == View::Menu) return menuKey(k);
  if (view_ == View::Link) {
    if (k.action == Action::Back || k.action == Action::Select || k.action == Action::Left)
      view_ = View::None;
    return;
  }
  if (reasonMode_) return reasonKey(k);
  typingKey(k);
}

void EntryScreen::typingKey(const KeyEvent& k) {
  if (spell_.key(k, input_)) return;
  switch (k.action) {
    case Action::Char:
      if (sending_) break;
      input_.insert(k.ch);
      spell_.typed(input_, spell::english(), app().cfg.spellCheck);
      break;
    case Action::Backspace:
      if (!sending_) input_.backspace();
      break;
    case Action::ClearLine:
      if (!sending_) input_.clear();
      break;
    case Action::Up:
      log_.up();
      break;
    case Action::Down:
      down();
      break;
    case Action::Left:
      input_.left();
      break;
    case Action::Right:
      input_.right();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Log) && log_.selectMessage(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Focus:
      input_.setCursor(k.index);
      break;
    case Action::Select:
      select();
      break;
    case Action::Options:
      openEntryMenu();
      break;
    case Action::Back:
      if (!log_.following()) log_.follow();
      else leaveToList();
      break;
    default:
      break;
  }
}

// Steps to newer blocks; at the end, asks for the next page of replies.
void EntryScreen::down() {
  if (!log_.following()) {
    log_.down();
    return;
  }
  if (!loaded_ || moreAsked_) return;
  bool end = lockedFeed().with([](FeedState& s) { return s.store.repliesEnd(); });
  if (end) return;
  moreAsked_ = true;
  feedSource().command(Command{CmdType::MoreReplies});
}

void EntryScreen::select() {
  if (!loaded_) return;
  if (!input_.text().empty()) {
    send();
    return;
  }
  if (sending_) return;
  std::string reply = selectedReply();
  if (!reply.empty()) {
    openReplyMenu(reply);
  } else if (hidden_ && !revealed_) {
    revealed_ = true;
    dirty_ = true;
  } else {
    openEntryMenu();
  }
}

void EntryScreen::send() {
  if (sending_) {
    ui::banner("Still sending\u2026", true);
    return;
  }
  std::string body = input_.utf8();
  std::string err = validateReply(body);
  if (!err.empty()) {
    ui::banner(err, true);
    return;
  }
  if (app().conn == 0) {
    ui::banner("Offline: not sent", true);
    return;
  }
  sending_ = true;
  feedSource().command(
      Command{CmdType::PostReply, replyJson(postId_, body, selectedReply())});
}

void EntryScreen::reasonKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Char:
      if (input_.text().size() < kReasonMaxChars) input_.insert(k.ch);
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
    case Action::Select: {
      std::string reason = input_.utf8();
      reasonMode_ = false;
      input_.setText(savedInput_);
      feedSource().command(
          Command{targetReply_ ? CmdType::FlagReply : CmdType::FlagEntry, target_, reason});
      break;
    }
    case Action::Back:
      reasonMode_ = false;
      input_.setText(savedInput_);
      break;
    default:
      break;
  }
}

// ---- menus ----

void EntryScreen::openMenu(Menu m, std::u32string title, std::vector<ListItem> items) {
  menu_ = m;
  // Every entry menu acts on the entry or a reply: buttons on touch-only boards.
  box_.openButtons(std::move(title), std::move(items));
  view_ = View::Menu;
}

void EntryScreen::openEntryMenu() {
  Entry e;
  bool found = false, marked = false, watching = false;
  lockedFeed().with([&](FeedState& s) {
    const Entry* p = s.store.entry();
    if (!p || p->id != postId_) return;
    found = true;
    e.author = p->author;
    e.slug = p->slug;
    e.deleted = p->deleted;
    marked = !s.store.bookmarkIdFor(postId_).empty();
    watching = s.store.watching();
  });
  if (!found) return;
  target_ = postId_;
  targetReply_ = false;
  url_ = entryUrl(e);
  std::vector<ListItem> items;
  menuKeys_.clear();
  auto add = [&](const char32_t* label, const char* key, ItemTone tone = ItemTone::Normal) {
    items.push_back(ListItem{label, U"", tone});
    menuKeys_.push_back(key);
  };
  if (!e.deleted) add(U"Reply", "reply");
  add(marked ? U"Unbookmark" : U"Bookmark", marked ? "unbookmark" : "bookmark");
  add(watching ? U"Unwatch" : U"Watch", watching ? "unwatch" : "watch");
  add(U"Show link", "link");
  if (!e.deleted) {
    if (isSelf(e.author)) add(U"Delete\u2026", "delete", ItemTone::Error);
    else add(U"Report\u2026", "report", ItemTone::Error);
  }
  const std::u32string sizeItem = ui::textSizeItem();
  add(sizeItem.c_str(), "size");
  openMenu(Menu::Entry, U"entry", std::move(items));
}

void EntryScreen::openReplyMenu(const std::string& replyId) {
  std::string author, link;
  bool found = false, deleted = false, marked = false;
  lockedFeed().with([&](FeedState& s) {
    const Entry* p = s.store.entry();
    if (!p || p->id != postId_) return;
    for (const auto& r : s.store.replies()) {
      if (r.id != replyId) continue;
      found = true;
      author = r.author;
      deleted = r.deleted;
      link = entryUrl(*p) + "#" + r.id;
      marked = !s.store.bookmarkIdFor(r.id).empty();
      break;
    }
  });
  if (!found) return;
  target_ = replyId;
  targetReply_ = true;
  url_ = link;
  std::vector<ListItem> items;
  menuKeys_.clear();
  auto add = [&](const char32_t* label, const char* key, ItemTone tone = ItemTone::Normal) {
    items.push_back(ListItem{label, U"", tone});
    menuKeys_.push_back(key);
  };
  if (!deleted) add(U"Reply", "reply");
  add(marked ? U"Unbookmark" : U"Bookmark", marked ? "unbookmark" : "bookmark");
  add(U"Show link", "link");
  if (!deleted) {
    if (isSelf(author)) add(U"Delete\u2026", "delete", ItemTone::Error);
    else add(U"Report\u2026", "report", ItemTone::Error);
  }
  openMenu(Menu::Reply, utf8ToU32(author.empty() ? std::string("reply") : author),
           std::move(items));
}

void EntryScreen::menuKey(const KeyEvent& k) {
  switch (box_.key(k)) {
    case Overlay::Result::Picked:
      pickMenu();
      break;
    case Overlay::Result::Closed:
      view_ = View::None;
      break;
    case Overlay::Result::None:
      break;
  }
}

void EntryScreen::pickMenu() {
  int i = box_.selected();
  if (i < 0 || i >= int(menuKeys_.size())) return;
  const std::string key = menuKeys_[i];
  view_ = View::None;
  switch (menu_) {
    case Menu::Entry:
    case Menu::Reply:
      if (key == "reply") {
        // The selection stays, so the next send answers this reply (or the entry).
        if (menu_ == Menu::Entry) log_.follow();
      } else if (key == "bookmark") {
        feedSource().command(
            Command{CmdType::Bookmark, postId_, targetReply_ ? target_ : std::string()});
      } else if (key == "unbookmark") {
        feedSource().command(Command{CmdType::Unbookmark, target_});
      } else if (key == "watch" || key == "unwatch") {
        feedSource().command(Command{key == "watch" ? CmdType::Watch : CmdType::Unwatch, postId_});
      } else if (key == "link") {
        link_.open(url_);
        view_ = View::Link;
      } else if (key == "report") {
        menuKeys_ = {"cancel", "report"};
        openMenu(Menu::ConfirmReport, targetReply_ ? U"report this reply?" : U"report this entry?",
                 {ListItem{U"Cancel", U""}, ListItem{U"Report\u2026", U"", ItemTone::Error}});
      } else if (key == "delete") {
        menuKeys_ = {"cancel", "delete"};
        openMenu(Menu::ConfirmDelete, targetReply_ ? U"delete this reply?" : U"delete this entry?",
                 {ListItem{U"Cancel", U""}, ListItem{U"Delete", U"", ItemTone::Error}});
      } else if (key == "size") {
        ui::cycleTextSize();
      }
      break;
    case Menu::ConfirmDelete:
      if (key == "delete")
        feedSource().command(
            Command{targetReply_ ? CmdType::DeleteReply : CmdType::DeleteEntry, target_});
      break;
    case Menu::ConfirmReport:
      if (key == "report") {
        savedInput_ = input_.text();
        input_.clear();
        reasonMode_ = true;
      }
      break;
  }
}

// ---- events ----

void EntryScreen::onEvent(const Event& e) {
  switch (e.type) {
    case EvtType::EntryUpdated:
      // Gone or failed to load: back to the list (leave() closes the entry).
      if (!e.ok) leaveToList();
      else dirty_ = true;
      break;
    case EvtType::RepliesLoaded:
      moreAsked_ = false;
      repliesArrived_ = true;
      dirty_ = true;
      break;
    case EvtType::FeedWriteDone:
      dirty_ = true;
      if (!sending_) break;
      sending_ = false;
      if (e.ok) {
        input_.clear();
        log_.follow();
      } else {
        // The text stays so it can be sent again by hand.
        ui::banner(e.text.empty() ? std::string("Not sent") : e.text, true);
      }
      break;
    default:
      break;
  }
}

// ---- drawing ----

void EntryScreen::rebuild(int cols, int logRows, const Theme& t) {
  const int page = std::max(1, logRows - 1);  // one line of the last page stays in view
  std::vector<DisplayLine> lines;
  std::map<std::string, std::string> authors;
  bool present = false;
  // Lay out under the lock: copying a long entry and its replies would cost
  // more memory than the layout takes time.
  lockedFeed().with([&](FeedState& s) {
    const Entry* e = s.store.entry();
    if (!e || e->id != postId_) return;
    present = true;
    hidden_ = !e->deleted && (!e->contentWarning.empty() || e->nsfw);
    title_ = utf8ToU32(e->title.empty() ? "@" + e->author : e->title);
    EntryLayoutOptions o;
    o.cols = cols;
    o.size = app().cfg.textSize;
    o.nowMs = nowEpochMs();
    o.self = app().user;
    o.revealed = revealed_;
    o.theme = &t;
    lines = layoutEntry(*e, s.store.replies(), o);
    for (const auto& r : s.store.replies()) authors[r.id] = r.author;
  });
  // A block's lines are consecutive; count them per block.
  std::string block;
  int n = 0;
  for (auto& l : lines) {
    if (l.divider) continue;
    if (l.messageId != block) {
      block = l.messageId;
      n = 0;
    }
    int p = n++ / page;
    if (p > 0) l.messageId = block + kPageSep + std::to_string(p);
  }
  // The entry opens at its top: the header and the first page of the body.
  if (present && !loaded_) {
    log_.setLines(std::move(lines));
    selectBlock(postId_);
  } else {
    log_.setLines(std::move(lines));
  }
  loaded_ = present;
  authors_ = std::move(authors);

  // A bookmarked reply: select it if the first page brought it.
  if (!wantReply_.empty() && repliesArrived_) {
    if (authors_.count(wantReply_)) selectBlock(wantReply_);
    wantReply_.clear();
  }
}

// LogView only steps, so walk up from the end to the block (its first page:
// the untagged id).
void EntryScreen::selectBlock(const std::string& id) {
  log_.follow();
  while (log_.selectedId() != id) {
    std::string before = log_.selectedId();
    log_.up();
    if (log_.selectedId() == before) break;
  }
  if (log_.selectedId() != id) log_.follow();
}

void EntryScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  const bool small = compact();
  const int inputRow = small ? g.rows() - 2 : g.rows() - 1;
  const int logRows = inputRow - 1;
  const int nav = sideNavWidth(g);
  const int logCols = g.cols() - nav - LogView::kBarCols;
  if (dirty_ || logCols != cols_ || logRows != logRows_ || app().cfg.textSize != size_ ||
      &t != theme_ || nowMs - builtAt_ >= kAgeRefreshMs) {
    rebuild(logCols, logRows, t);
    dirty_ = false;
    cols_ = logCols;
    logRows_ = logRows;
    size_ = app().cfg.textSize;
    theme_ = &t;
    builtAt_ = nowMs;
  }

  g.fill(t.bg);
  drawStatus(g, t, title_.empty() ? std::u32string(U"entry") : title_, -1, true);
  if (view_ == View::Link) {
    link_.render(g, t);
    return;
  }

  int logBottom = inputRow;
  std::string reply = selectedReply();
  if (!reply.empty()) {
    // The reply the input line answers.
    --logBottom;
    auto it = authors_.find(reply);
    putLine(g, 1, logBottom, "\u21b3 @" + (it == authors_.end() ? std::string("?") : it->second),
            t.dim, t.bg);
  }
  if (spell_.visible() && view_ == View::None && !reasonMode_) spell_.render(g, --logBottom, t);
  const bool offline = app().conn == 0;
  if (loaded_) log_.render(g, 1, logBottom, nav, t);
  else putLine(g, 1, 2, offline ? "offline" : "loading\u2026", t.dim, t.bg);

  if (view_ == View::Menu)
    box_.render(g, 1, logBottom, nav, OverlayPlace::BottomRight, t);
  if (loaded_ && nav > 0) renderSideNav(g, 1, logBottom, nav, t);

  input_.setDisabled(offline || view_ != View::None || sending_ || !loaded_);
  input_.setEmojiButton(true);
  input_.render(g, inputRow, t, reasonMode_ ? U"reason> " : U"> ");

  if (small) {
    const char32_t* hint;
    const char32_t* touch;
    if (spell_.visible()) {
      hint = U"\u2190\u2192 spelling  \u25CF keep";
      touch = U"tap a spelling, or keep typing";
    } else if (reasonMode_) {
      hint = U"type a reason (optional)  \u25CF report  back: cancel";
      touch = U"type a reason (optional)  enter: report";
    } else if (view_ == View::Menu) {
      hint = U"\u2191\u2193 pick  \u25CF choose  back: close";
      touch = U"tap: choose  outside: close";
    } else if (sending_) {
      hint = touch = U"sending\u2026";
    } else if (moreAsked_) {
      hint = touch = U"loading replies\u2026";
    } else if (offline) {
      hint = touch = U"offline: reading only";
    } else if (!reply.empty()) {
      hint = U"type to answer  \u25CF reply menu  back: unselect";
      touch = U"type to answer  \u2039 unselect";
    } else if (hidden_ && !revealed_) {
      hint = U"\u25CF reveal  \u2191\u2193 scroll";
      touch = U"tap to reveal  tap a reply for actions";
    } else if (!log_.following()) {
      hint = U"\u25CF send / menu  \u2191\u2193 scroll  back: unselect";
      touch = U"tap the line to type  tap a reply for actions";
    } else {
      hint = U"\u25CF send / menu  \u2191\u2193 scroll  back: feed";
      touch = U"tap the line to type  tap a reply for actions";
    }
    drawHint(g, t, hint, touch);
  }
}

}  // namespace rc
