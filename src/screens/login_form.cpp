#include <algorithm>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "screens/draw.h"
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

void LoginFormScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"log in", -1);
  putLine(g, 1, 2, "Cyberspace email", t.dim, t.bg);
  email_.setDisabled(focus_ != 0 || busy_);
  email_.render(g, 3, t);
  putLine(g, 1, 5, "Password", t.dim, t.bg);
  InputLine mask;
  mask.setText(std::u32string(password_.text().size(), U'\u2022'));
  mask.setFieldId(1);
  mask.setCursor(password_.cursor());
  mask.setDisabled(focus_ != 1 || busy_);
  mask.render(g, 6, t);
  int r = 8;
  if (!note_.empty()) r = putWrapped(g, 1, r, note_, busy_ ? t.fg : t.error, t.bg) + 1;
  putWrapped(g, 1, std::max(r, 10), "The password is sent once and never stored.", t.dim, t.bg);
  drawHint(g, t, U"\u2191\u2193 field  \u25CF next/sign in  back: cancel",
           U"tap field  enter: next/sign in");
}

}  // namespace rc
