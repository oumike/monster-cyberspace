#include "screens/composer.h"

#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/compose.h"
#include "feed/feed_source.h"
#include "screens/draw.h"

namespace rc {

namespace {

// The draft outlives the screen; only Post and Discard clear it.
EntryDraft gDraft;
bool gPosting = false;  // a PostEntry waits for its result

bool hasText() {
  return !gDraft.title.empty() || !gDraft.body.empty() || !gDraft.topics.empty() ||
         !gDraft.cw.empty();
}

// True when the event settles the post in flight.
bool settle(const Event& e) {
  if (!gPosting) return false;
  if (e.type == EvtType::FeedWriteDone && !e.ok) {
    gPosting = false;  // the draft stays, to post again by hand
    ui::banner(e.text.empty() ? std::string("Not posted") : e.text, true);
    return true;
  }
  if (e.type == EvtType::EntryPosted) {
    gPosting = false;
    gDraft = EntryDraft{};
    return true;
  }
  return false;
}

}  // namespace

void composerBackgroundEvent(const Event& e) { settle(e); }

void ComposerScreen::enter(const std::string&) {
  menu_ = Menu::None;
  load();
}

void ComposerScreen::load() {
  title_.setText(utf8ToU32(gDraft.title));
  topics_.setText(utf8ToU32(gDraft.topics));
  cw_.setText(utf8ToU32(gDraft.cw));
  body_.clear();
  for (char32_t c : utf8ToU32(gDraft.body)) body_.insert(c);
  if (!hasText()) focus_ = kTitle;
}

void ComposerScreen::save() {
  switch (focus_) {
    case kTitle: gDraft.title = title_.utf8(); break;
    case kBody: gDraft.body = body_.utf8(); break;
    case kTopics: gDraft.topics = topics_.utf8(); break;
    case kCw: gDraft.cw = cw_.utf8(); break;
    default: break;
  }
}

InputLine* ComposerScreen::line() {
  switch (focus_) {
    case kTitle: return &title_;
    case kTopics: return &topics_;
    case kCw: return &cw_;
    default: return nullptr;
  }
}

// ---- keys ----

void ComposerScreen::onKey(const KeyEvent& k) {
  if (menu_ != Menu::None) return menuKey(k);
  fieldKey(k);
}

void ComposerScreen::fieldKey(const KeyEvent& k) {
  if (k.action == Action::Back || k.action == Action::OpenFeed) return askLeave();
  if (gPosting) return;  // no edits while the post is in flight
  InputLine* in = line();
  switch (k.action) {
    case Action::Char:
      if (in) in->insert(k.ch);
      else if (focus_ == kBody) body_.insert(k.ch);
      save();
      break;
    case Action::Backspace:
      if (in) in->backspace();
      else if (focus_ == kBody) body_.backspace();
      save();
      break;
    case Action::ClearLine:
      if (in) in->clear();
      save();
      break;
    case Action::Left:
      if (in) in->left();
      else if (focus_ == kBody) body_.left();
      break;
    case Action::Right:
      if (in) in->right();
      else if (focus_ == kBody) body_.right();
      break;
    case Action::Up:
      // In Body, Up moves by line first; form fields don't wrap.
      if (focus_ == kBody && body_.up(cols_)) break;
      if (focus_ > kTitle) --focus_;
      break;
    case Action::Down:
      if (focus_ == kBody && body_.down(cols_)) break;
      if (focus_ < kCw) ++focus_;
      break;
    case Action::Select:
      if (focus_ == kBody) {
        body_.insert(U'\n');
        save();
      } else if (focus_ == kPublic) {
        gDraft.isPublic = !gDraft.isPublic;
      } else if (focus_ == kNsfw) {
        gDraft.nsfw = !gDraft.nsfw;
      } else if (focus_ < kCw) {
        ++focus_;
      }
      break;
    case Action::Palette: {
      const Theme& t = currentTheme();
      openMenu(Menu::Actions, U"entry",
               {ListItem{U"Post\u2026", U"", t.fg}, ListItem{U"Discard\u2026", U"", t.error}});
      break;
    }
    default:
      break;
  }
}

void ComposerScreen::askLeave() {
  // A post in flight keeps the draft until it settles.
  if (gPosting) return router::go(router::Id::FeedList);
  if (!hasText()) return discard();
  const Theme& t = currentTheme();
  openMenu(Menu::ConfirmDiscard, U"Discard draft?",
           {ListItem{U"Cancel", U"", t.fg}, ListItem{U"Discard", U"", t.error}});
}

void ComposerScreen::discard() {
  gDraft = EntryDraft{};
  load();
  focus_ = kTitle;
  router::go(router::Id::FeedList);
}

// ---- menus ----

void ComposerScreen::openMenu(Menu m, std::u32string title, std::vector<ListItem> items) {
  menu_ = m;
  menuTitle_ = std::move(title);
  menuList_.setItems(std::move(items));
  menuList_.setSelected(0);  // Post in the actions menu; Cancel in a confirm
}

void ComposerScreen::menuKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      menuList_.up();
      break;
    case Action::Down:
      menuList_.down();
      break;
    case Action::Back:
    case Action::Left:
    case Action::Palette:
      menu_ = Menu::None;
      break;
    case Action::Select:
    case Action::Right:
      pick();
      break;
    default:
      break;
  }
}

void ComposerScreen::pick() {
  const int i = menuList_.selected();
  const Menu m = menu_;
  menu_ = Menu::None;
  switch (m) {
    case Menu::Actions:
      if (i == 0) {
        post();
      } else if (!hasText()) {
        discard();
      } else {
        askLeave();
      }
      break;
    case Menu::ConfirmPost:
      if (i != 1) break;
      if (gPosting) break;
      if (app().conn == 0) {
        ui::banner("Offline: not posted", true);
        break;
      }
      gPosting = true;
      feedSource().command(Command{CmdType::PostEntry, entryJson(gDraft)});
      break;
    case Menu::ConfirmDiscard:
      if (i == 1) discard();
      break;
    case Menu::None:
      break;
  }
}

void ComposerScreen::post() {
  if (gPosting) {
    ui::banner("Still posting\u2026", true);
    return;
  }
  std::string err = validateDraft(gDraft);
  if (!err.empty()) {
    ui::banner(err, true);
    return;
  }
  if (app().conn == 0) {
    ui::banner("Offline: not posted", true);
    return;
  }
  const Theme& t = currentTheme();
  openMenu(Menu::ConfirmPost, U"Post entry? (2 per minute, 24 per day)",
           {ListItem{U"Cancel", U"", t.fg}, ListItem{U"Post", U"", t.fg}});
}

// ---- events ----

void ComposerScreen::onEvent(const Event& e) {
  if (!settle(e) || e.type != EvtType::EntryPosted) return;
  load();  // the cleared draft
  focus_ = kTitle;
  router::go(router::Id::Entry, e.text);
}

// ---- drawing ----

void ComposerScreen::render(Grid& g, const Theme& t, int64_t) {
  const bool small = app().cfg.textSize == TextSize::Compact;
  const int rows = g.rows();
  cols_ = g.cols();
  g.fill(t.bg);
  drawStatus(g, t, U"new entry", -1);

  // Rows: title, the body label, the body, topics, the checkboxes, CW, hint.
  const int hintRows = small ? 1 : 0;
  const int cwRow = rows - 1 - hintRows;
  const int checkRow = cwRow - 1;
  const int topicsRow = checkRow - 1;
  const int titleRow = 1, labelRow = 2, bodyTop = 3;
  const int bodyBottom = std::max(bodyTop + 1, topicsRow);

  title_.setDisabled(focus_ != kTitle || gPosting);
  title_.render(g, titleRow, t, U"Title ");

  const uint16_t labelFg = focus_ == kBody ? t.fg : t.dim;
  g.fillRow(labelRow, t.bg);
  putLine(g, 0, labelRow, "Body", labelFg, t.bg);
  std::string count = std::to_string(body_.length()) + "/4096";
  putLine(g, std::max(0, g.cols() - int(count.size())), labelRow, count, t.dim, t.bg);
  body_.render(g, bodyTop, bodyBottom, t, focus_ == kBody && !gPosting);

  topics_.setDisabled(focus_ != kTopics || gPosting);
  topics_.render(g, topicsRow, t, U"Topics ");

  g.fillRow(checkRow, t.bg);
  auto box = [&](int col, bool on, const char32_t* label, bool focused) {
    std::u32string s = std::u32string(on ? U"[x] " : U"[ ] ") + label;
    uint16_t fg = focused ? t.selectFg : t.fg, bg = focused ? t.selectBg : t.bg;
    return col + g.putText(col, checkRow, s, fg, bg, g.cols() - col);
  };
  int col = box(0, gDraft.isPublic, U"Public", focus_ == kPublic);
  box(col + 2, gDraft.nsfw, U"NSFW", focus_ == kNsfw);

  cw_.setDisabled(focus_ != kCw || gPosting);
  cw_.render(g, cwRow, t, U"CW ");

  if (menu_ != Menu::None) {
    const int n = int(menuList_.size());
    const int width = std::min(g.cols() - 2, std::max(20, int(menuTitle_.size()) + 2));
    const int left = std::max(1, (g.cols() - width) / 2);
    const int top = std::max(1, (rows - n - 1) / 2);
    for (int c = 0; c < width; ++c) g.put(left + c, top, U' ', t.barFg, t.barBg);
    g.putText(left + 1, top, menuTitle_, t.barFg, t.barBg, width - 2);
    menuList_.render(g, top + 1, std::min(rows, top + 1 + n), left, width, t);
  }

  if (small) {
    const char32_t* hint;
    if (menu_ != Menu::None) hint = U"\u2191\u2193 pick  \u25CF choose  back: close";
    else if (gPosting) hint = U"posting\u2026";
    else if (focus_ == kBody) hint = U"\u25CF new line  \u2191\u2193 lines  ctrl post  back: feed";
    else if (focus_ == kPublic || focus_ == kNsfw)
      hint = U"\u25CF toggle  \u2191\u2193 fields  ctrl post  back: feed";
    else hint = U"\u2191\u2193 fields  ctrl post  back: feed";
    drawHint(g, t, hint);
  }
}

}  // namespace rc
