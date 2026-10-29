#include "screens/composer.h"

#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/compose.h"
#include "feed/feed_source.h"
#include "screens/draw.h"
#include "ui/tap_map.h"

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
  if (!gPosting || e.type != EvtType::EntryPosted) return false;
  gPosting = false;
  if (e.ok) gDraft = EntryDraft{};
  else  // the draft stays, to post again by hand
    ui::banner(e.text.empty() ? std::string("Not posted") : e.text, true);
  return true;
}

}  // namespace

void composerBackgroundEvent(const Event& e) { settle(e); }

void ComposerScreen::enter(const std::string&) {
  menu_ = Menu::None;
  spell_.hide();
  title_.setFieldId(kTitle);
  body_.setFieldId(kBody);
  topics_.setFieldId(kTopics);
  cw_.setFieldId(kCw);
  load();
}

bool ComposerScreen::acceptsText() const {
  if (menu_ != Menu::None || gPosting) return false;
  return focus_ == kTitle || focus_ == kBody || focus_ == kTopics || focus_ == kCw;
}

bool ComposerScreen::consumesBackspace() const {
  if (!acceptsText()) return false;
  if (focus_ == kBody) return body_.length() > 0;
  const InputLine* in = focus_ == kTitle ? &title_ : focus_ == kTopics ? &topics_ : &cw_;
  return !in->text().empty();
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
  if (focus_ == kTitle ? spell_.key(k, title_) : focus_ == kBody && spell_.key(k, body_)) {
    save();
    return;
  }
  if (k.action == Action::Back || k.action == Action::OpenFeed) return askLeave();
  if (gPosting) return;  // no edits while the post is in flight
  InputLine* in = line();
  switch (k.action) {
    case Action::Char:
      if (in) in->insert(k.ch);
      else if (focus_ == kBody) body_.insert(k.ch);
      // Titles and bodies are prose; topics and the CW are tags and labels.
      if (focus_ == kTitle) spell_.typed(title_, spell::english(), app().cfg.spellCheck);
      else if (focus_ == kBody) spell_.typed(body_, spell::english(), app().cfg.spellCheck);
      else spell_.hide();
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
    case Action::Palette:
    case Action::Options: {
      openMenu(Menu::Actions, U"entry",
               {ListItem{U"Post\u2026", U""}, ListItem{U"Discard\u2026", U"", ItemTone::Error}});
      break;
    }
    case Action::Focus:
      if (k.target == kPublic) {
        focus_ = kPublic;
        gDraft.isPublic = !gDraft.isPublic;
      } else if (k.target == kNsfw) {
        focus_ = kNsfw;
        gDraft.nsfw = !gDraft.nsfw;
      } else if (k.target < kFieldCount) {
        focus_ = k.target;
        if (InputLine* f = line()) f->setCursor(k.index);
        else if (focus_ == kBody) body_.setCursor(k.index);
      }
      break;
    default:
      break;
  }
}

void ComposerScreen::askLeave() {
  // A post in flight keeps the draft until it settles.
  if (gPosting) return router::go(router::Id::FeedList);
  if (!hasText()) return discard();
  openMenu(Menu::ConfirmDiscard, U"Discard draft?",
           {ListItem{U"Cancel", U""}, ListItem{U"Discard", U"", ItemTone::Error}});
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
  box_.open(std::move(title), std::move(items));  // Post in the actions menu; Cancel in a confirm
}

void ComposerScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) {
    menu_ = Menu::None;
    box_.close();
    return;
  }
  switch (box_.key(k)) {
    case Overlay::Result::Picked:
      pick();
      break;
    case Overlay::Result::Closed:
      menu_ = Menu::None;
      break;
    case Overlay::Result::None:
      break;
  }
}

void ComposerScreen::pick() {
  const int i = box_.selected();
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
  openMenu(Menu::ConfirmPost, U"Post entry? (2 per minute, 24 per day)",
           {ListItem{U"Cancel", U""}, ListItem{U"Post", U""}});
}

// ---- events ----

void ComposerScreen::onEvent(const Event& e) {
  if (!settle(e) || !e.ok) return;
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
  drawStatus(g, t, U"new entry", -1, true);

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
  // Suggestions sit between the title and the body, over the label.
  if (spell_.visible() && (focus_ == kTitle || focus_ == kBody) && menu_ == Menu::None)
    spell_.render(g, labelRow, t);

  topics_.setDisabled(focus_ != kTopics || gPosting);
  topics_.render(g, topicsRow, t, U"Topics ");

  g.fillRow(checkRow, t.bg);
  auto box = [&](int col, bool on, const char32_t* label, bool focused) {
    std::u32string s = std::u32string(on ? U"[x] " : U"[ ] ") + label;
    const bool fill = focused && !t.outlined;
    uint16_t fg = fill ? t.selectFg : t.fg, bg = fill ? t.selectBg : t.bg;
    const int w = g.putText(col, checkRow, s, fg, bg, g.cols() - col);
    if (focused && t.outlined && w > 0) g.outline(col, checkRow, col + w - 1, checkRow, t.selectBg);
    return col + w;
  };
  int col = box(0, gDraft.isPublic, U"Public", focus_ == kPublic);
  int nsfwStart = col + 2;
  int nsfwEnd = box(nsfwStart, gDraft.nsfw, U"NSFW", focus_ == kNsfw);
  if (activeTaps()) {
    Target tp;
    tp.kind = TargetKind::Field;
    tp.field = kPublic;
    activeTaps()->addCells(0, checkRow, std::max(0, col - 1), checkRow, tp);
    Target tn;
    tn.kind = TargetKind::Field;
    tn.field = kNsfw;
    activeTaps()->addCells(nsfwStart, checkRow, std::max(nsfwStart, nsfwEnd - 1), checkRow, tn);
  }

  cw_.setDisabled(focus_ != kCw || gPosting);
  cw_.render(g, cwRow, t, U"CW ");

  if (menu_ != Menu::None)
    box_.render(g, 0, rows, 0, OverlayPlace::Centre, t);

  if (small) {
    const char32_t* hint;
    const char32_t* touch;
    if (menu_ != Menu::None) {
      hint = U"\u2191\u2193 pick  \u25CF choose  back: close";
      touch = U"tap: choose  outside: close";
    } else if (gPosting) {
      hint = touch = U"posting\u2026";
    } else if (spell_.visible() && (focus_ == kTitle || focus_ == kBody)) {
      hint = U"\u2191\u2193 spelling  \u25CF keep";
      touch = U"tap a spelling, or keep typing";
    } else if (focus_ == kBody) {
      hint = U"\u25CF new line  \u2191\u2193 lines  ctrl post  back: feed";
      touch = U"\u25CF new line  \u2191\u2193 lines  \u22ef post  \u2039 feed";
    } else if (focus_ == kPublic || focus_ == kNsfw) {
      hint = U"\u25CF toggle  \u2191\u2193 fields  ctrl post  back: feed";
      touch = U"tap to toggle  \u22ef post  \u2039 feed";
    } else {
      hint = U"\u2191\u2193 fields  ctrl post  back: feed";
      touch = U"tap a field  \u22ef post  \u2039 feed";
    }
    drawHint(g, t, hint, touch);
  }
}

}  // namespace rc
