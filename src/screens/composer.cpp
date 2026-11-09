#include "feature_flags.h"
#if RC_FEAT_COMPOSER

#include "screens/composer.h"

#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/compose.h"
#include "feed/feed_source.h"
#include "hal/board.h"
#include "lvui/input.h"
#include "lvui/styles.h"
#include "lvui/views/view_util.h"
#include "screens/lv_draw.h"
#include "store/locked_feed.h"

namespace rc {

namespace {

// The draft outlives the screen; only Post and Discard clear it.
EntryDraft gDraft;
bool gPosting = false;  // a PostEntry waits for its result
std::string gGuildSlug;  // set: a new thread in that guild (Title, Body, Topics only)

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

void ComposerScreen::enter(const std::string& key) {
  menu_ = Menu::None;
  toMap_ = toFeed_ = false;
  gGuildSlug = key.rfind("guild:", 0) == 0 ? key.substr(6) : std::string();
  spell_.hide();
  title_.setFieldId(kTitle);
  body_.setFieldId(kBody);
  topics_.setFieldId(kTopics);
  cw_.setFieldId(kCw);
  load();
  if (!gGuildSlug.empty() && focus_ > kTopics) focus_ = kTitle;
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
  if (k.action == Action::Back) return askLeave(false, false);
  if (k.action == Action::OpenFeed) return askLeave(false, true);
  if (k.action == Action::OpenMap) return askLeave(true, false);
  if (gPosting) return;  // no edits while the post is in flight
  InputLine* in = line();
  switch (k.action) {
    case Action::Char:
      if (in) in->insert(k.ch);
      else if (focus_ == kBody) body_.insert(k.ch);
      // Titles and bodies are prose; topics and the CW are tags and labels.
#if RC_FEAT_SPELL
      if (focus_ == kTitle) spell_.typed(title_, spell::english(), app().cfg.spellCheck);
      else if (focus_ == kBody) spell_.typed(body_, spell::english(), app().cfg.spellCheck);
      else spell_.hide();
#endif
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
      if (focus_ < (gGuildSlug.empty() ? kCw : kTopics)) ++focus_;
      break;
    case Action::Select:
      if (focus_ == kBody) {
        body_.insert(U'\n');
        save();
      } else if (focus_ == kPublic) {
        gDraft.isPublic = !gDraft.isPublic;
      } else if (focus_ == kNsfw) {
        gDraft.nsfw = !gDraft.nsfw;
      } else if (focus_ < (gGuildSlug.empty() ? kCw : kTopics)) {
        ++focus_;
      }
      break;
    case Action::Palette:
    case Action::Options: {
      openMenu(Menu::Actions, gGuildSlug.empty() ? U"entry" : U"thread",
               {ListItem{U"Post\u2026", U""}, ListItem{U"Discard\u2026", U"", ItemTone::Error}});
      break;
    }
    case Action::Focus:
      if (!gGuildSlug.empty() && k.target > kTopics) break;
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

void ComposerScreen::goBack() {
  if (toMap_) router::go(router::Id::GuildList);
  else if (!gGuildSlug.empty() && !toFeed_) router::go(router::Id::Guild, gGuildSlug);
  else router::go(router::Id::FeedList);
}

void ComposerScreen::askLeave(bool toMap, bool toFeed) {
  toMap_ = toMap;
  toFeed_ = toFeed;
  // A post in flight keeps the draft until it settles.
  if (gPosting) return goBack();
  if (!hasText()) return discard();
  openMenu(Menu::ConfirmDiscard, U"Discard draft?",
           {ListItem{U"Cancel", U""}, ListItem{U"Discard", U"", ItemTone::Error}});
}

void ComposerScreen::discard() {
  gDraft = EntryDraft{};
  load();
  focus_ = kTitle;
  goBack();
}

// ---- menus ----

void ComposerScreen::openMenu(Menu m, std::u32string title, std::vector<ListItem> items) {
  menu_ = m;
  box_.open(std::move(title), std::move(items));  // Post in the actions menu; Cancel in a confirm
}

void ComposerScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) {
    menu_ = Menu::None;
    toMap_ = toFeed_ = false;
    box_.close();
    return;
  }
  switch (box_.key(k)) {
    case Overlay::Result::Picked:
      pick();
      break;
    case Overlay::Result::Closed:
      menu_ = Menu::None;
      toMap_ = toFeed_ = false;
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
        askLeave(false, false);
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
      if (gGuildSlug.empty())
        feedSource().command(Command{CmdType::PostEntry, entryJson(gDraft)});
      else
        feedSource().command(Command{CmdType::PostGuildThread, gGuildSlug, guildThreadJson(gDraft)});
      break;
    case Menu::ConfirmDiscard:
      if (i == 1) discard();
      else toMap_ = toFeed_ = false;
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
  openMenu(Menu::ConfirmPost,
           gGuildSlug.empty() ? U"Post entry? (2 per minute, 24 per day)"
                              : U"Post thread? (2 per minute, 24 per day)",
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

ComposerScreen::Rows ComposerScreen::layout(int rows, bool small, bool guild) {
  // Rows: title, the body label, the body, topics, the checkboxes, CW, hint
  // (a guild thread has only title, body and topics).
  Rows r;
  const int hintRows = small ? 1 : 0;
  r.cw = guild ? rows : rows - 1 - hintRows;
  r.check = guild ? rows : r.cw - 1;
  r.topics = guild ? rows - 1 - hintRows : r.check - 1;
  // Touch boards pad the title with a row above and below, so it's easier
  // to hit; a tap on the padding puts the cursor at the end.
  r.pad = board::hasTouch() ? 1 : 0;
  r.title = 1 + r.pad;
  r.label = r.title + 1 + r.pad;
  r.bodyTop = r.label + 1;
  r.bodyBottom = std::max(r.bodyTop + 1, r.topics);
  return r;
}

std::u32string ComposerScreen::statusTitle(bool guild) const {
  if (!guild) return U"new entry";
  std::string name = lockedFeed().with([](FeedState& s) {
    const Guild* gd = s.guilds.guild();
    return gd ? (gd->name.empty() ? gd->slug : gd->name) : std::string();
  });
  if (name.empty()) name = gGuildSlug;
  return U"new thread \u00b7 " + utf8ToU32(name);
}

void ComposerScreen::hints(bool guild, const char32_t*& hint, const char32_t*& touch) const {
  if (menu_ != Menu::None) {
    hint = U"\u2191\u2193 pick  \u25CF choose  back: close";
    touch = U"tap: choose  outside: close";
  } else if (gPosting) {
    hint = touch = U"posting\u2026";
  } else if (spell_.visible() && (focus_ == kTitle || focus_ == kBody)) {
    hint = U"\u2190\u2192 spelling  \u25CF keep";
    touch = U"tap a spelling, or keep typing";
  } else if (focus_ == kBody) {
    hint = guild ? U"\u25CF new line  \u2191\u2193 lines  ctrl post  back: guild"
                 : U"\u25CF new line  \u2191\u2193 lines  ctrl post  back: feed";
    touch = guild ? U"\u25CF new line  \u2191\u2193 lines  \u22ef post  \u2039 guild"
                  : U"\u25CF new line  \u2191\u2193 lines  \u22ef post  \u2039 feed";
  } else if (focus_ == kPublic || focus_ == kNsfw) {
    hint = U"\u25CF toggle  \u2191\u2193 fields  ctrl post  back: feed";
    touch = U"tap to toggle  \u22ef post  \u2039 feed";
  } else {
    hint = guild ? U"\u2191\u2193 fields  ctrl post  back: guild"
                 : U"\u2191\u2193 fields  ctrl post  back: feed";
    touch = guild ? U"tap a field  \u22ef post  \u2039 guild" : U"tap a field  \u22ef post  \u2039 feed";
  }
}

// ---- LVGL ----

void ComposerScreen::FieldTap::tapped(lv_obj_t*, bool) {
  KeyEvent e;
  e.action = Action::Focus;
  e.index = index;
  e.target = field;
  lvui::emit(e);
}

namespace {

// A bare touch target with nothing drawn.
lv_obj_t* tapBox(lv_obj_t* parent, lvui::Tappable* v) {
  lv_obj_t* o = lvui::plainBox(parent);
  lvui::addTap(o, v);
  return o;
}

}  // namespace

void ComposerScreen::build(lv_obj_t* root) {
  root_ = root;
  titleTap_.field = kTitle;
  publicField_.field = kPublic;
  nsfwField_.field = kNsfw;
  text_.create(root);
  padAbove_ = tapBox(root, &titleTap_);
  lvui::addStyle(padAbove_, lvui::Role::Input);
  padBelow_ = tapBox(root, &titleTap_);
  lvui::addStyle(padBelow_, lvui::Role::Input);
  bodyView_.create(root);
  titleView_.create(root);
  topicsView_.create(root);
  cwView_.create(root);
  promptTap_ = tapBox(root, &titleTap_);
  checkFocus_ = lvui::plainBox(root);
  lvui::addStyle(checkFocus_, lvui::Role::Focused);
  publicTap_ = tapBox(root, &publicField_);
  nsfwTap_ = tapBox(root, &nsfwField_);
  spellView_.create(root);
  hint_.create(root);
  status_.create(root);
  menuView_.create(root);
  sync();
}

void ComposerScreen::update(int64_t nowMs) {
  // The draft changes only with keys, and the post's result with EntryPosted,
  // both counted by ui::inputSeq().
  if (gate_.due(root_, nowMs, {int64_t(focus_), int64_t(menu_), int64_t(gPosting)})) sync();
}

void ComposerScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const bool small = app().cfg.textSize == TextSize::Compact;
  const int rows = text_.rows(), cols = text_.cols();
  cols_ = cols - TextArea::kBarCols;
  const bool guild = !gGuildSlug.empty();
  syncStatus(status_, statusTitle(guild), -1, true);
  const Rows r = layout(rows, small, guild);

  // The title, padded on touch boards; the padding's target puts the cursor
  // at the end, as does a tap on the prompt.
  title_.setDisabled(focus_ != kTitle || gPosting);
  titleTap_.index = static_cast<int16_t>(title_.text().size());
  lvui::setHidden(padAbove_, !r.pad);
  lvui::setHidden(padBelow_, !r.pad);
  lvui::setHidden(promptTap_, !r.pad);
  if (r.pad) {
    placeCells(padAbove_, 0, r.title - 1, cols, r.title);
    placeCells(padBelow_, 0, r.title + 1, cols, r.title + 2);
    placeCells(promptTap_, 0, r.title, int(std::u32string_view(U"Title ").size()), r.title + 1);
  }
  placeCells(titleView_.obj(), 0, r.title, cols, r.title + 1);
  titleView_.sync(title_, U"Title ");

  text_.begin();
  text_.line(0, r.label, "Body", focus_ == kBody ? t.fg : t.dim);
  std::string count = std::to_string(body_.length()) + "/4096";
  text_.line(std::max(0, cols - int(count.size())), r.label, count, t.dim);
  placeCells(bodyView_.obj(), 0, r.bodyTop, cols, r.bodyBottom);
  bodyView_.sync(body_, focus_ == kBody && !gPosting);

  topics_.setDisabled(focus_ != kTopics || gPosting);
  placeCells(topicsView_.obj(), 0, r.topics, cols, r.topics + 1);
  topicsView_.sync(topics_, U"Topics ");

  // The checkboxes: a focused one in selection colours, or outlined.
  bool outline = false;
  lvui::setHidden(publicTap_, guild);
  lvui::setHidden(nsfwTap_, guild);
  if (!guild) {
    auto box = [&](int col, bool on, const char32_t* label, bool focused) {
      std::u32string s = std::u32string(on ? U"[x] " : U"[ ] ") + label;
      const int w = focused && !t.outlined ? text_.filled(col, r.check, s, t.selectFg, t.selectBg)
                                           : text_.line(col, r.check, s, t.fg, cols - col);
      if (focused && t.outlined && w > 0) {
        placeCells(checkFocus_, col, r.check, w, r.check + 1);
        outline = true;
      }
      return col + w;
    };
    const int col = box(0, gDraft.isPublic, U"Public", focus_ == kPublic);
    const int nsfwStart = col + 2;
    const int nsfwEnd = box(nsfwStart, gDraft.nsfw, U"NSFW", focus_ == kNsfw);
    placeCells(publicTap_, 0, r.check, std::max(1, col), r.check + 1);
    placeCells(nsfwTap_, nsfwStart, r.check, std::max(1, nsfwEnd - nsfwStart), r.check + 1);
  }
  text_.end();
  lvui::setHidden(checkFocus_, !outline);

  lvui::setHidden(cwView_.obj(), guild);
  if (!guild) {
    cw_.setDisabled(focus_ != kCw || gPosting);
    placeCells(cwView_.obj(), 0, r.cw, cols, r.cw + 1);
    cwView_.sync(cw_, U"CW ");
  }

#if RC_FEAT_SPELL
  // Suggestions sit between the title and the body, over the label.
  if (spell_.visible() && (focus_ == kTitle || focus_ == kBody) && menu_ == Menu::None) {
    placeCells(spellView_.obj(), 0, r.label, cols, r.label + 1);
    spellView_.sync(spell_);
  } else {
    lvui::setHidden(spellView_.obj(), true);
  }
#else
  lvui::setHidden(spellView_.obj(), true);
#endif

  menuView_.setPlace(OverlayPlace::Centre, rows * text_.cell().h, 0, 0);
  menuView_.sync(box_);

  lvui::setHidden(hint_.obj(), !small);
  if (small) {
    const char32_t* hint;
    const char32_t* touch;
    hints(guild, hint, touch);
    syncHint(hint_, hint, touch);
  }
}

}  // namespace rc

#endif  // RC_FEAT_COMPOSER
