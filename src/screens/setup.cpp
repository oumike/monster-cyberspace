#include "app/app_state.h"
#include "net/wifi_mgr.h"
#include "screens/draw.h"
#include "screens/screens.h"

namespace rc {

void SetupScreen::enter(const std::string&) {
  menu_.setItems({ListItem{U"Log in on this device", U"", currentTheme().fg}});
}

void SetupScreen::onKey(const KeyEvent& k) {
  if (k.action == Action::Select || k.action == Action::Right)
    router::go(router::Id::LoginForm);
}

void SetupScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"setup", -1);
  int r = 2;
#if !RC_FAKE_API
  if (wifimgr::mode() == wifimgr::Mode::Setup) {
    putLine(g, 1, r++, "Join this Wi-Fi network:", t.fg, t.bg);
    putLine(g, 3, r++, wifimgr::apSsid(), t.selectBg, t.bg);
    putLine(g, 3, r++, wifimgr::apHasPassword() ? "password required" : "open network", t.dim,
            t.bg);
    putLine(g, 1, r++, "then browse to:", t.fg, t.bg);
    putLine(g, 3, r++, "http://192.168.4.1", t.selectBg, t.bg);
    ++r;
  }
#endif
  if (wifiUp()) {
    putLine(g, 1, r++, "On " + wifiSsid() + ", browse to:", t.fg, t.bg);
    putLine(g, 3, r++, "http://rhino-cyberspace.local", t.selectBg, t.bg);
    putLine(g, 3, r++, "http://" + wifiIp(), t.selectBg, t.bg);
    ++r;
  }
  if (!app().cfg.wifi.empty() && app().auth != 1 && app().auth != 2) {
    putLine(g, 1, r++, "Sign in at rhino-cyberspace.local", t.fg, t.bg);
    putLine(g, 1, r++, "or on this device.", t.fg, t.bg);
  }
  int bottom = g.rows() - 1;
  menu_.render(g, bottom - 1, bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u25CF select");
}

}  // namespace rc
