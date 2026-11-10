#include "feature_flags.h"
#if RC_FEAT_PEOPLE

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feed/feed_source.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/locked_people.h"
#include "ui/nick_color.h"

namespace rc {

namespace {

const char32_t* const kPrompts[] = {U"Name     ", U"Bio      ", U"Website  ", U"Site name",
                                    U"Location "};

std::u32string prompt(int i) { return std::u32string(kPrompts[i]) + U" > "; }

}  // namespace

void ProfileEditScreen::enter(const std::string&) {
  box_.close();
  busy_ = false;
  focus_ = 0;
  Profile p;
  lockedPeople().with([&](PeopleState& s) {
    if (s.loaded && sameUsername(s.profile.username, app().user)) p = s.profile;
  });
  before_ = profileFields(p);
  const std::string* vals[kFields] = {&before_.displayName, &before_.bio, &before_.websiteUrl,
                                      &before_.websiteName, &before_.locationName};
  for (int i = 0; i < kFields; ++i) {
    fields_[i].setText(utf8ToU32(*vals[i]));
    fields_[i].setFieldId(uint8_t(i));
  }
}

ProfileFields ProfileEditScreen::current() const {
  return ProfileFields{fields_[0].utf8(), fields_[1].utf8(), fields_[2].utf8(), fields_[3].utf8(),
                       fields_[4].utf8()};
}

void ProfileEditScreen::save() {
  const ProfileFields after = current();
  if (const char* err = profileFieldError(after)) {
    ui::banner(err, true);
    return;
  }
  std::string json = profilePatchJson(before_, after);
  if (json.empty()) {
    router::go(router::Id::Profile, app().user);
    return;
  }
  busy_ = true;
  Command c{CmdType::SaveProfile};
  c.a = std::move(json);
  c.b = app().user;
  feedSource().command(c);
}

void ProfileEditScreen::leave() {
  if (!busy_ && !profilePatchJson(before_, current()).empty()) {
    box_.confirm(U"Discard changes?", U"Discard");
    return;
  }
  router::go(router::Id::Profile, app().user);
}

void ProfileEditScreen::onKey(const KeyEvent& k) {
  if (box_.isOpen()) {
    if (box_.key(k) == Overlay::Result::Picked && box_.selected() == 1)
      router::go(router::Id::Profile, app().user);
    return;
  }
  if (busy_) {
    if (k.action == Action::Back) leave();
    return;
  }
  InputLine& in = fields_[focus_];
  switch (k.action) {
    case Action::Char:
      in.insert(k.ch);
      break;
    case Action::Backspace:
      in.backspace();
      break;
    case Action::ClearLine:
      in.clear();
      break;
    case Action::Left:
      in.left();
      break;
    case Action::Right:
      in.right();
      break;
    case Action::Up:
      focus_ = (focus_ + kFields - 1) % kFields;
      break;
    case Action::Down:
      focus_ = (focus_ + 1) % kFields;
      break;
    case Action::Focus:
      if (k.target < kFields) {
        focus_ = k.target;
        fields_[focus_].setCursor(k.index);
      }
      break;
    case Action::Select:
      if (focus_ < kFields - 1) ++focus_;
      else save();
      break;
    case Action::Palette:
      save();
      break;
    case Action::Back:
      leave();
      break;
    default:
      break;
  }
}

void ProfileEditScreen::onEvent(const Event& e) {
  if (e.type != EvtType::ProfileSaved || !busy_) return;
  busy_ = false;
  if (e.ok) {
    ui::banner("Profile saved", false);
    router::go(router::Id::Profile, app().user);
  } else {
    ui::banner(e.text.empty() ? std::string("Couldn't save") : e.text, true);
  }
}

// ---- LVGL ----

void ProfileEditScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->text_.create(root);
  for (auto& f : v_->fieldViews_) f.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  v_->menuView_.create(root);
  sync();
}

void ProfileEditScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->text_.obj(), nowMs)) sync();
}

void ProfileEditScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(v_->status_, U"edit profile", -1);
  const int cols = v_->text_.cols(), rows = v_->text_.rows();
  for (int i = 0; i < kFields; ++i) {
    fields_[i].setDisabled(i != focus_ || busy_);
    placeCells(v_->fieldViews_[i].obj(), 0, 1 + i, cols, 2 + i);
    v_->fieldViews_[i].sync(fields_[i], prompt(i));
  }
  v_->text_.begin();
  if (busy_) v_->text_.line(1, kFields + 2, "Saving\u2026", t.fg);
  else v_->text_.wrapped(1, kFields + 2, "Empty a field to clear it.", t.dim);
  v_->text_.end();
  v_->menuView_.setPlace(OverlayPlace::Centre, rows * v_->text_.cell().h, 0, 0);
  v_->menuView_.sync(box_);
  if (busy_) syncHint(v_->hint_, U"saving\u2026", U"saving\u2026");
  else
    syncHint(v_->hint_, U"\u2191\u2193 field  \u25CF next/save  back: cancel",
             U"tap a field  enter: next/save");
}

}  // namespace rc

#endif  // RC_FEAT_PEOPLE
