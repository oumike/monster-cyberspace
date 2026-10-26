#include <Arduino.h>

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/settings.h"

namespace rc {

namespace {
enum Item { kSetupAp, kLogout, kReset };
constexpr int kItems = 3;
}

void SettingsScreen::enter(const std::string&) {
  confirm_ = Confirm::None;
  menu_.setSelected(0);
}

void SettingsScreen::onKey(const KeyEvent& k) {
  ListView& m = confirm_ != Confirm::None ? confirmMenu_ : menu_;
  switch (k.action) {
    case Action::Up:
      m.up();
      break;
    case Action::Down:
      m.down();
      break;
    case Action::Back:
    case Action::Left:
      if (confirm_ != Confirm::None) confirm_ = Confirm::None;
      else router::go(router::Id::Home);
      break;
    case Action::Select:
    case Action::Right:
      if (confirm_ != Confirm::None) {
        Confirm c = confirm_;
        confirm_ = Confirm::None;
        if (confirmMenu_.selected() != 1) break;
        if (c == Confirm::Logout) {
          chatSource().command(Command{CmdType::Logout});
        } else {
          settings::factoryReset();
          delay(100);
          ESP.restart();
        }
      } else if (menu_.selected() == kSetupAp) {
        Command c{CmdType::SetupAp};
        c.n = setupApUp() ? 0 : 1;
        chatSource().command(c);
      } else {
        confirm_ = menu_.selected() == kLogout ? Confirm::Logout : Confirm::Reset;
        confirmMenu_.setSelected(0);
      }
      break;
    default:
      break;
  }
}

void SettingsScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"settings", -1);
  const AppState& a = app();
  int r = 1;
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    putLine(g, 1, r, label, t.dim, t.bg);
    putLine(g, 11, r++, value, fg, t.bg);
  };
  field("account", a.user.empty() ? "not signed in" : "@" + a.user, t.fg);
  field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
  field("config", "http://rhino-cyberspace.local", t.fg);
  int bat = board::batteryPercent();
  field("battery", bat < 0 ? "unknown" : std::to_string(bat) + "%", t.fg);
  bool ap = setupApUp();
  field("setup AP", ap ? "on" : "off", ap ? t.selectBg : t.fg);
  if (ap) {
    field("", setupApSsid(), t.selectBg);
    field("", setupApHasPassword() ? "password required" : "open network", t.dim);
    field("", "http://192.168.4.1", t.selectBg);
  }

  menu_.setItems({
      ListItem{ap ? U"Turn setup AP off" : U"Turn setup AP on", U"", t.fg},
      ListItem{U"Log out", U"", t.fg},
      ListItem{U"Factory reset\u2026", U"", t.error},
  });
  int bottom = g.rows() - 1;
  if (confirm_ != Confirm::None) {
    bool reset = confirm_ == Confirm::Reset;
    confirmMenu_.setItems({ListItem{U"Cancel", U"", t.fg},
                           ListItem{reset ? U"Erase everything" : U"Log out", U"", t.error}});
    putLine(g, 1, bottom - 3,
            reset ? "Erase Wi-Fi, account and settings?" : "Log out of Cyberspace?", t.error,
            t.bg);
    confirmMenu_.render(g, bottom - 2, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel");
  } else {
    menu_.render(g, bottom - kItems, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF select  back: home");
  }
}

}  // namespace rc
