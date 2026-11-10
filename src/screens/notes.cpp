#include "feature_flags.h"
#if RC_FEAT_NOTES

#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/locked_notes.h"
#include "ui/feed_layout.h"

namespace rc {

namespace {

std::u32string age(int64_t ms) {
  return ms >= 0 ? utf8ToU32(relativeAge(ms, nowEpochMs())) : std::u32string();
}

// A NoteEdit's text outlives the screen until it is saved or discarded.
struct NoteDraft {
  std::string id;  // the note it edits; "" for a new one
  std::string text;
  bool set = false;
} gNoteDraft;

}  // namespace

// ---- the list and the history ----

void NoteListScreen::enter(const std::string& id) {
  id_ = id;
  failed_ = false;
  list_.setItems({});
  list_.setSelected(0);
  request(false);
}

void NoteListScreen::request(bool append) {
  asked_ = append;
  loading_ = true;
  Command c{history_ ? CmdType::OpenNoteHistory : CmdType::OpenNotes};
  c.a = id_;
  c.n = append ? 1 : 0;
  feedSource().command(c);
}

void NoteListScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
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
      if (i < 0 || i >= int(keys_.size())) break;
      if (!history_ && i == 0) router::go(router::Id::NoteEdit, "");
      else router::go(router::Id::Note, keys_[i]);
      break;
    }
    case Action::Left:
    case Action::Back:
      if (history_) {
        router::go(router::Id::Note, id_);
      } else {
        feedSource().command(Command{CmdType::CloseNotes});
        router::go(router::Id::Home);
      }
      break;
    default:
      break;
  }
}

void NoteListScreen::onEvent(const Event& e) {
  if (e.type != (history_ ? EvtType::NoteHistoryUpdated : EvtType::NotesUpdated)) return;
  asked_ = loading_ = false;
  failed_ = !e.ok;
}

std::vector<ListItem> NoteListScreen::rows() {
  std::vector<NoteRow> notes;
  lockedNotes().with([&](NotesState& s) {
    if (history_) {
      if (s.historyId == id_) notes = s.history;
      more_ = s.historyId == id_ && !s.historyCursor.empty();
    } else {
      notes = s.list;
      more_ = !s.listCursor.empty();
    }
  });
  std::vector<ListItem> items;
  keys_.clear();
  if (!history_) {
    items.push_back(ListItem{U"+ New note", U"", ItemTone::Normal});
    keys_.push_back("");
  }
  for (const auto& n : notes) {
    ListItem it;
    if (history_) {
      it.label = utf8ToU32("rev " + std::to_string(n.revision));
      keys_.push_back(id_ + "\n" + std::to_string(n.revision));
    } else {
      it.label = n.title.empty() ? std::u32string(U"(empty)") : utf8ToU32(n.title);
      keys_.push_back(n.id);
    }
    it.right = age(n.updatedMs);
    items.push_back(std::move(it));
  }
  return items;
}

void NoteListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->navView_.create(root);
  v_->text_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync();
}

void NoteListScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().conn), loading_ + 2 * failed_ + 4 * asked_}))
    sync();
}

void NoteListScreen::sync() {
  const Theme& t = lvui::currentTheme();
  std::vector<ListItem> items = rows();
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const int nav = sideNavWidth(v_->text_);
  syncStatus(v_->status_, history_ ? U"note history" : U"notes", -1);
  const bool empty = items.size() == (history_ ? 0u : 1u);
  const int sel = list_.selected();
  list_.setItems(std::move(items));
  list_.setSelected(sel);
  placeCells(v_->listView_.obj(), 0, 1, cols - nav, bottom);
  v_->listView_.sync(list_);
  v_->text_.begin();
  if (empty) {
    const char* text = loading_ && app().conn == 0 ? "offline"
                       : failed_                   ? "couldn't load"
                       : loading_                  ? "loading\u2026"
                       : history_                  ? "no revisions"
                                                   : "no notes yet";
    v_->text_.line(1, history_ ? 2 : 3, text, t.dim);
  }
  v_->text_.end();
  v_->navView_.sync(nav, 1, bottom, cols);
  if (asked_) syncHint(v_->hint_, U"loading more\u2026", U"loading more\u2026");
  else if (history_)
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 note", U"tap a revision  \u2039 note");
  else
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap a note  \u2039 home");
}

// ---- one note ----

void NoteScreen::enter(const std::string& arg) {
  closeOverlays();
  const size_t nl = arg.find('\n');
  id_ = arg.substr(0, nl);
  revision_ = nl == std::string::npos ? 0 : std::max(0, std::atoi(arg.c_str() + nl + 1));
  loaded_ = failed_ = busy_ = false;
  text_.setText(U"");
  Command c{CmdType::OpenNote, id_};
  c.n = revision_;
  feedSource().command(c);
}

void NoteScreen::onKey(const KeyEvent& k) {
  if (box_.isOpen()) return menuKey(k);
  switch (k.action) {
    case Action::Up:
      text_.up();
      break;
    case Action::Down:
      text_.down();
      break;
    case Action::Select:
    case Action::Right:
    case Action::Palette:
    case Action::Options:
      if (revision_ == 0 && loaded_ && !busy_)
        box_.open(U"note", {ListItem{U"Edit", U""}, ListItem{U"History", U""},
                            ListItem{U"Delete\u2026", U"", ItemTone::Error}});
      break;
    case Action::Left:
    case Action::Back:
      if (revision_ == 0) router::go(router::Id::Notes);
      else router::go(router::Id::NoteHistory, id_);
      break;
    default:
      break;
  }
}

void NoteScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) return closeOverlays();
  if (box_.key(k) != Overlay::Result::Picked) {
    if (!box_.isOpen()) confirm_ = false;
    return;
  }
  const int i = box_.selected();
  if (confirm_) {
    confirm_ = false;
    if (i != 1) return;
    if (app().conn == 0) {
      ui::banner("Offline: not deleted", true);
      return;
    }
    busy_ = true;
    feedSource().command(Command{CmdType::DeleteNote, id_});
    return;
  }
  switch (i) {
    case 0: router::go(router::Id::NoteEdit, id_); break;
    case 1: router::go(router::Id::NoteHistory, id_); break;
    case 2:
      confirm_ = true;
      box_.confirm(U"Delete this note and all its revisions?", U"Delete");
      break;
    default: break;
  }
}

void NoteScreen::onEvent(const Event& e) {
  if (e.type == EvtType::NoteUpdated) {
    std::string content;
    bool mine = false;
    lockedNotes().with([&](NotesState& s) {
      mine = s.open.id == id_ && (revision_ == 0 || s.open.revision == revision_);
      if (mine) content = s.open.content;
    });
    if (e.ok && mine) {
      loaded_ = true;
      text_.setText(utf8ToU32(content));
    } else if (!e.ok) {
      failed_ = true;
    }
  } else if (e.type == EvtType::NoteDeleted && busy_) {
    busy_ = false;
    if (e.ok) {
      ui::banner("Note deleted", false);
      router::go(router::Id::Notes);
    } else {
      ui::banner(e.text.empty() ? std::string("Couldn't delete") : e.text, true);
    }
  }
}

void NoteScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->textView_.create(root);
  v_->cells_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  sync();
}

void NoteScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->cells_.obj(), nowMs, {int64_t(app().conn), loaded_ + 2 * failed_ + 4 * busy_}))
    sync();
}

void NoteScreen::sync() {
  const Theme& t = lvui::currentTheme();
  int rev = 0, latest = 0;
  int64_t updated = -1;
  lockedNotes().with([&](NotesState& s) {
    if (s.open.id != id_) return;
    rev = s.open.revision;
    updated = s.open.updatedMs;
    latest = s.latestRevision;
  });
  std::u32string title = U"note";
  if (loaded_ && rev > 0) {
    title += U" \u00b7 rev " + utf8ToU32(std::to_string(rev));
    if (revision_ != 0 && latest > 0) title += U" of " + utf8ToU32(std::to_string(latest));
    if (updated >= 0) title += U" \u00b7 " + age(updated);
  }
  syncStatus(v_->status_, title, -1, revision_ == 0 && loaded_);
  const int cols = v_->cells_.cols(), rows = v_->cells_.rows(), bottom = rows - 1;
  lvui::setHidden(v_->textView_.obj(), !loaded_);
  v_->cells_.begin();
  if (loaded_) {
    placeCells(v_->textView_.obj(), 0, 1, cols, bottom);
    v_->textView_.sync(text_);
  } else {
    v_->cells_.line(1, 2, failed_ ? (app().conn == 0 ? "offline" : "couldn't load") : "loading\u2026",
                    t.dim);
  }
  v_->cells_.end();
  if (box_.isOpen()) {
    const lvui::Cell c = v_->cells_.cell();
    v_->menuView_.setPlace(OverlayPlace::Centre, rows * c.h, 0, 0);
    v_->menuView_.sync(box_);
  } else {
    lvui::setHidden(v_->menuView_.backdrop(), true);
  }
  if (box_.isOpen()) {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  } else if (busy_) {
    syncHint(v_->hint_, U"deleting\u2026", U"deleting\u2026");
  } else if (revision_ == 0) {
    syncHint(v_->hint_, U"\u2191\u2193 scroll  \u25CF actions  \u2190 notes",
             U"swipe to scroll  \u22ef actions  \u2039 notes");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 scroll  \u2190 history", U"swipe to scroll  \u2039 history");
  }
}

// ---- writing ----

void NoteEditScreen::enter(const std::string& id) {
  closeOverlays();
  saving_ = false;
  id_ = id;
  // Keep a draft of this same note; else start from the note (or empty).
  if (!gNoteDraft.set || gNoteDraft.id != id) {
    gNoteDraft = NoteDraft{id, "", true};
    if (!id.empty())
      lockedNotes().with([&](NotesState& s) {
        if (s.open.id == id) gNoteDraft.text = s.open.content;
      });
  }
  body_.clear();
  for (char32_t c : utf8ToU32(gNoteDraft.text)) body_.insert(c);
}

void NoteEditScreen::onKey(const KeyEvent& k) {
  if (menu_ != Menu::None) return menuKey(k);
  if (k.action == Action::Back) return leave();
  if (saving_) return;
  switch (k.action) {
    case Action::Char:
      body_.insert(k.ch);
      break;
    case Action::Backspace:
      body_.backspace();
      break;
    case Action::Left:
      body_.left();
      break;
    case Action::Right:
      body_.right();
      break;
    case Action::Up:
      body_.up(cols_);
      break;
    case Action::Down:
      body_.down(cols_);
      break;
    case Action::Select:
      body_.insert(U'\n');
      break;
    case Action::Focus:
      body_.setCursor(k.index);
      break;
    case Action::Palette:
    case Action::Options:
      menu_ = Menu::Actions;
      box_.open(id_.empty() ? U"new note" : U"edit note",
                {ListItem{U"Save", U""}, ListItem{U"Discard\u2026", U"", ItemTone::Error}});
      return;
    default:
      return;
  }
  gNoteDraft.text = body_.utf8();
}

void NoteEditScreen::menuKey(const KeyEvent& k) {
  if (k.action == Action::Palette) return closeOverlays();
  const Overlay::Result r = box_.key(k);
  if (r == Overlay::Result::Closed) menu_ = Menu::None;
  if (r != Overlay::Result::Picked) return;
  const Menu m = menu_;
  const int i = box_.selected();
  menu_ = Menu::None;
  if (m == Menu::Actions) {
    if (i == 0) save();
    else if (body_.length() == 0) discard();
    else {
      menu_ = Menu::ConfirmDiscard;
      box_.confirm(U"Discard changes?", U"Discard");
    }
  } else if (m == Menu::ConfirmDiscard && i == 1) {
    discard();
  }
}

void NoteEditScreen::save() {
  if (saving_) return;
  const std::string text = body_.utf8();
  if (const char* err = noteError(text)) {
    ui::banner(err, true);
    return;
  }
  if (app().conn == 0) {
    ui::banner("Offline: not saved", true);
    return;
  }
  saving_ = true;
  feedSource().command(Command{CmdType::SaveNote, id_, noteBodyJson(text)});
}

void NoteEditScreen::leave() {
  if (saving_) {
    router::go(id_.empty() ? router::Id::Notes : router::Id::Note, id_);
    return;
  }
  std::string original;
  if (!id_.empty())
    lockedNotes().with([&](NotesState& s) {
      if (s.open.id == id_) original = s.open.content;
    });
  if (body_.utf8() == original) return discard();
  menu_ = Menu::ConfirmDiscard;
  box_.confirm(U"Discard changes?", U"Discard");
}

void NoteEditScreen::discard() {
  gNoteDraft = NoteDraft{};
  body_.clear();
  if (id_.empty()) router::go(router::Id::Notes);
  else router::go(router::Id::Note, id_);
}

void NoteEditScreen::onEvent(const Event& e) {
  if (e.type != EvtType::NoteSaved || !saving_) return;
  saving_ = false;
  if (!e.ok) {
    ui::banner(e.text.empty() ? std::string("Not saved") : e.text, true);
    return;
  }
  gNoteDraft = NoteDraft{};
  body_.clear();
  ui::banner("Note saved", false);
  router::go(router::Id::Note, e.text);
}

void NoteEditScreen::build(lv_obj_t* root) {
  v_.make(root);
  body_.setFieldId(0);
  v_->text_.create(root);
  v_->bodyView_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  sync();
}

void NoteEditScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(menu_), int64_t(saving_)})) sync();
}

void NoteEditScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const int rows = v_->text_.rows(), cols = v_->text_.cols();
  cols_ = cols - TextArea::kBarCols;
  syncStatus(v_->status_, id_.empty() ? U"new note" : U"edit note", -1, true);
  v_->text_.begin();
  const std::string count = std::to_string(body_.length()) + "/" + std::to_string(kNoteMaxChars);
  v_->text_.line(std::max(0, cols - int(count.size())), 1, count, t.dim);
  v_->text_.end();
  placeCells(v_->bodyView_.obj(), 0, 2, cols, std::max(3, rows - 1));
  v_->bodyView_.sync(body_, menu_ == Menu::None && !saving_);
  v_->menuView_.setPlace(OverlayPlace::Centre, rows * v_->text_.cell().h, 0, 0);
  v_->menuView_.sync(box_);
  if (menu_ != Menu::None)
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF choose  back: close", U"tap: choose  outside: close");
  else if (saving_)
    syncHint(v_->hint_, U"saving\u2026", U"saving\u2026");
  else
    syncHint(v_->hint_, U"\u25CF new line  \u2191\u2193 lines  ctrl save  back: cancel",
             U"\u25CF new line  \u22ef save  \u2039 cancel");
}

}  // namespace rc

#endif  // RC_FEAT_NOTES
