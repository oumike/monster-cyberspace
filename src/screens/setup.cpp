#include <utility>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"

namespace rc {

void SetupScreen::enter(const std::string&) {
  std::vector<ListItem> items{ListItem{U"Log in on this device", U""}};
  hasRestore_ = false;
  if (board::hasSdSlot()) {
    std::vector<backupstore::FileInfo> files;
    const backupstore::Result r = backupstore::listSd(files);
    if (r.ok && !files.empty()) {
      items.push_back(ListItem{U"Restore from SD card", U""});
      hasRestore_ = true;
    }
  }
  menu_.setItems(std::move(items));
  menu_.setSelected(0);
}

void SetupScreen::onKey(const KeyEvent& k) {
  if (picker_.isOpen()) return picker_.onKey(k);
  switch (k.action) {
    case Action::Up:
      menu_.up();
      break;
    case Action::Down:
      menu_.down();
      break;
    case Action::Select:
    case Action::Right:
      if (hasRestore_ && menu_.selected() == 1) picker_.open();
      else router::go(router::Id::LoginForm);
      break;
    default:
      break;
  }
}

void SetupScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"setup", -1);
  if (picker_.isOpen()) return picker_.render(g, t);
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
  const int n = int(menu_.size());
  menu_.render(g, bottom - n, bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF select");
}

}  // namespace rc
