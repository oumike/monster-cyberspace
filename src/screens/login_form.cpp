#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"

namespace rc {

void LoginFormScreen::enter(const std::string&) {
  email_.setText(utf8ToU32(app().cfg.email));
  password_.clear();
  focus_ = email_.text().empty() ? 0 : 1;
  busy_ = false;
  note_.clear();
}

void LoginFormScreen::leave() { password_.clear(); }

void LoginFormScreen::onKey(const KeyEvent& k) {
  if (busy_) return;
  InputLine& in = focus_ == 0 ? email_ : password_;
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
      if (focus_ == 0) in.left();
      break;
    case Action::Right:
      if (focus_ == 0) in.right();
      break;
    case Action::Up:
      focus_ = 0;
      break;
    case Action::Down:
      focus_ = 1;
      break;
    case Action::Focus:
      focus_ = k.target == 1 ? 1 : 0;
      (focus_ == 0 ? email_ : password_).setCursor(k.index);
      break;
    case Action::Back:
      router::go(router::Id::Setup);
      break;
    case Action::Select:
      if (focus_ == 0) {
        focus_ = 1;
      } else if (email_.text().empty() || password_.text().empty()) {
        note_ = "Enter your email and password";
      } else {
        Command c{CmdType::Login, email_.utf8(), password_.utf8()};
        password_.clear();
        busy_ = true;
        note_ = "Signing in\u2026";
        chatSource().command(c);
        std::fill(c.b.begin(), c.b.end(), '\0');
      }
      break;
    default:
      break;
  }
}

void LoginFormScreen::onEvent(const Event& e) {
  if (e.type != EvtType::AuthState) return;
  busy_ = false;
  if (!e.ok) note_ = e.text;
  else if (e.code == 2) note_ = "Verify your email, then come back";
}

void LoginFormScreen::build(lv_obj_t* root) {
  text_.create(root);
  emailView_.create(root);
  passwordView_.create(root);
  hint_.create(root);
  status_.create(root);
  sync();
}

void LoginFormScreen::update(int64_t nowMs) {
  if (gate_.due(text_.obj(), nowMs)) sync();
}

void LoginFormScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(status_, U"log in", -1);
  text_.begin();
  text_.line(1, 2, "Cyberspace email", t.dim);
  email_.setDisabled(focus_ != 0 || busy_);
  placeCells(emailView_.obj(), 0, 3, text_.cols(), 4);
  emailView_.sync(email_);
  text_.line(1, 5, "Password", t.dim);
  mask_.setText(std::u32string(password_.text().size(), U'\u2022'));
  mask_.setFieldId(1);
  mask_.setCursor(password_.cursor());
  mask_.setDisabled(focus_ != 1 || busy_);
  placeCells(passwordView_.obj(), 0, 6, text_.cols(), 7);
  passwordView_.sync(mask_);
  int r = 8;
  if (!note_.empty()) r = text_.wrapped(1, r, note_, busy_ ? t.fg : t.error) + 1;
  text_.wrapped(1, std::max(r, 10), "The password is sent once and never stored.", t.dim);
  text_.end();
  syncHint(hint_, U"\u2191\u2193 field  \u25CF next/sign in  back: cancel",
           U"tap field  enter: next/sign in");
}

}  // namespace rc
