// SPDX-License-Identifier: GPL-3.0-or-later
// Web config where it is an opt-in (RC_WEB_OPT_IN, Linux): Settings' "Web
// config page". The page is served only while this is on, and only with a
// page password, so the password is set here before it can turn on.
#include "feature_flags.h"

#if RC_WEB_OPT_IN

#include <algorithm>

#include "app/app_state.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/settings.h"
#include "ui/wrap.h"
#include "util/utf8.h"
#include "web/web_server.h"

namespace rc {

namespace {

constexpr size_t kMinPassword = 8, kMaxPassword = 63;

void wipe(std::string& s) {
  std::fill(s.begin(), s.end(), '\0');
  s.clear();
}

bool printable(const std::string& s) {
  return std::all_of(s.begin(), s.end(), [](char c) { return c >= 0x20 && c <= 0x7E; });
}

std::string url() {
  return wifiUp() ? "http://" + wifiIp() + ":" + std::to_string(web::kPort) : std::string();
}

}  // namespace

void WebConfigScreen::enter(const std::string&) {
  password_.clear();
  Settings s;
  settings::load(s);
  hasPassword_ = !s.pagePassword.empty();
  wipe(s.pagePassword);
  wipe(s.apPassword);
  wipe(s.refreshToken);
  note_.clear();
  noteIsError_ = false;
}

void WebConfigScreen::leave() { password_.clear(); }

void WebConfigScreen::turn(bool on) {
  app().cfg.webConfig = on;
  settings::saveWebConfig(on);
  web::apply();
  if (on && !web::running()) {
    note_ = "Couldn't start the page: is port " + std::to_string(web::kPort) + " in use?";
    noteIsError_ = true;
  }
}

void WebConfigScreen::select() {
  const bool on = app().cfg.webConfig;
  if (!password_.text().empty()) {
    std::string pw = password_.utf8();
    password_.clear();
    if (pw.size() < kMinPassword || pw.size() > kMaxPassword || !printable(pw)) {
      wipe(pw);
      note_ = "8 to 63 printable characters";
      noteIsError_ = true;
      return;
    }
    Settings s;
    settings::load(s);
    settings::saveSecurity(pw, s.apPassword);
    wipe(pw);
    wipe(s.pagePassword);
    wipe(s.apPassword);
    wipe(s.refreshToken);
    hasPassword_ = true;
    note_ = on ? "Password changed" : "Password saved";
    noteIsError_ = false;
    if (!on) turn(true);
    return;
  }
  if (on) {
    turn(false);
    note_.clear();
  } else if (hasPassword_) {
    note_.clear();
    noteIsError_ = false;
    turn(true);
  } else {
    note_ = "Type a page password first";
    noteIsError_ = true;
  }
}

void WebConfigScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Char:
      if (password_.text().size() < kMaxPassword) password_.insert(k.ch);
      break;
    case Action::Backspace:
      password_.backspace();
      break;
    case Action::ClearLine:
      password_.clear();
      break;
    case Action::Focus:
      password_.setCursor(k.index);
      break;
    case Action::Select:
      select();
      break;
    case Action::Back:
      if (!password_.text().empty())
        password_.clear();
      else if (!router::backToOrigin())
        router::go(router::Id::Settings);
      break;
    default:
      break;
  }
}

void WebConfigScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->text_.create(root);
  v_->passwordView_.create(root);
  v_->qr_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync();
}

void WebConfigScreen::update(int64_t nowMs) {
  if (v_->gate_.due(v_->text_.obj(), nowMs, {int64_t(app().cfg.webConfig), int64_t(web::running())})) sync();
}

void WebConfigScreen::sync() {
  const Theme& t = lvui::currentTheme();
  const bool on = app().cfg.webConfig && web::running();
  syncStatus(v_->status_, U"web config page", -1);
  v_->text_.begin();
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  const std::string link = url();
  // The QR code goes on the right when it fits beside 24 columns of text,
  // else below the text if there is room there, else nowhere.
  const bool encoded = on && !link.empty() && v_->qr_.sync(link);
  const int qrH = encoded ? v_->qr_.rows() : 0, qrW = encoded ? v_->qr_.cols() : 0;
  const bool qrRight = encoded && qrW + 25 <= cols && 1 + qrH <= bottom;
  const int w = qrRight ? cols - qrW - 1 : cols;
  int r = 1;
  auto put = [&](std::string_view text, uint16_t fg) {
    for (const auto& l : wrapText(utf8ToU32(text), w - 2, w - 2))
      if (r < bottom) v_->text_.line(1, r++, l, fg, w - 2);
  };
  put(on ? "On" : "Off", on ? t.selectBg : t.fg);
  if (on) {
    put(link.empty() ? "No network: join one in the system's settings" : link, t.fg);
    put("user admin", t.dim);
  }
  put(hasPassword_ ? "New page password (set)" : "Page password", t.dim);
  mask_.setText(std::u32string(password_.text().size(), U'\u2022'));
  mask_.setFieldId(0);
  mask_.setCursor(password_.cursor());
  placeCells(v_->passwordView_.obj(), 0, r, w, r + 1);
  v_->passwordView_.sync(mask_);
  r += 1;
  if (!note_.empty()) put(note_, noteIsError_ ? t.error : t.fg);
  const lvui::Cell c = v_->text_.cell();
  bool qrShown = false;
  if (qrRight) {
    lvui::place(v_->qr_.obj(), (cols - qrW) * c.w, 1 * c.h, qrW * c.w, qrH * c.h);
    qrShown = true;
  } else if (encoded && r + qrH <= bottom && qrW <= cols) {
    lvui::place(v_->qr_.obj(), (cols - qrW) / 2 * c.w, r * c.h, qrW * c.w, qrH * c.h);
    qrShown = true;
  }
  if (v_->qr_.obj()) lvui::setHidden(v_->qr_.obj(), !qrShown);
  v_->text_.end();
  const bool typed = !password_.text().empty();
  syncHint(v_->hint_,
           typed ? U"\u25CF save password  back: cancel"
                 : on ? U"\u25CF turn off  type: new password  back"
                      : U"\u25CF turn on  type: password  back",
           typed ? U"enter: save password" : on ? U"enter: turn off" : U"enter: turn on");
}

}  // namespace rc

#endif  // RC_WEB_OPT_IN
