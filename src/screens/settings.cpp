#include <Arduino.h>

#include "app/app_state.h"
#include "app/time_service.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/settings.h"
#include "tz/zones.h"

namespace rc {

namespace {
enum Item { kTheme, kZone, kSetupAp, kLogout, kReset };
constexpr int kItems = 5;

std::u32string widen(const char* s) {
  std::u32string out;
  for (; *s; ++s) out += static_cast<char32_t>(static_cast<unsigned char>(*s));
  return out;
}
}  // namespace

void SettingsScreen::enter(const std::string&) {
  confirm_ = Confirm::None;
  picking_ = false;
  menu_.setSelected(0);
}

void SettingsScreen::openZones() {
  const std::string current = timesvc::snapshot().tzName;
  std::vector<ListItem> items;
  int sel = 0;
  for (size_t i = 0; i < zoneCount(); ++i) {
    const Zone& z = zoneAt(i);
    ListItem it{widen(z.label), U""};
    if (current == z.iana) {
      it.marker = true;
      sel = int(i);
    }
    items.push_back(std::move(it));
  }
  zoneList_.setItems(std::move(items));
  zoneList_.setSelected(sel);
  picking_ = true;
}

void SettingsScreen::zoneKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      zoneList_.up();
      break;
    case Action::Down:
      zoneList_.down();
      break;
    case Action::Back:
    case Action::Left:
      picking_ = false;
      break;
    case Action::Select:
    case Action::Right: {
      picking_ = false;
      const int i = zoneList_.selected();
      if (i < 0 || size_t(i) >= zoneCount()) break;
      const Zone& z = zoneAt(size_t(i));
      settings::saveTz(z.posix, z.iana);
      timesvc::settingsChanged();
      break;
    }
    default:
      break;
  }
}

void SettingsScreen::onKey(const KeyEvent& k) {
  if (picking_) return zoneKey(k);
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
      } else if (menu_.selected() == kTheme) {
        AppState& a = app();
        a.cfg.theme = nextTheme(a.cfg.theme);
        settings::saveUiPrefs(a.cfg);
      } else if (menu_.selected() == kZone) {
        openZones();
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
  const int bottom = g.rows() - 1;
  // Status lines stop above the menu.
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    if (r >= bottom - kItems) return;
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
  const timesvc::Snapshot ts = timesvc::snapshot();
  field("gps", ts.gps, t.fg);
  field("time", ts.time, t.fg);
  field("zone", ts.zone, t.fg);

  menu_.setItems({
      ListItem{U"Theme: " + widen(themeLabel(a.cfg.theme)), U""},
      ListItem{U"Time zone: " + widen(zoneLabel(ts.tzName)), U""},
      ListItem{ap ? U"Turn setup AP off" : U"Turn setup AP on", U""},
      ListItem{U"Log out", U""},
      ListItem{U"Factory reset\u2026", U"", ItemTone::Error},
  });
  if (picking_) {
    // Drawn over the status lines and the menu, below the status bar.
    const int top = 1;
    for (int c = 0; c < g.cols(); ++c) g.put(c, top, U' ', t.barFg, t.barBg);
    g.putText(1, top, U"time zone", t.barFg, t.barBg, g.cols() - 2);
    for (int row = top + 1; row < bottom; ++row)
      for (int c = 0; c < g.cols(); ++c) g.put(c, row, U' ', t.fg, t.bg);
    zoneList_.render(g, top + 1, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF set  back: cancel");
  } else if (confirm_ != Confirm::None) {
    bool reset = confirm_ == Confirm::Reset;
    confirmMenu_.setItems({ListItem{U"Cancel", U""},
                           ListItem{reset ? U"Erase everything" : U"Log out", U"", ItemTone::Error}});
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
