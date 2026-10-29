#include <algorithm>
#include <utility>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "ui/button_grid.h"

namespace rc {

void SetupScreen::enter(const std::string&) {
  std::vector<ListItem> items{ListItem{U"Join Wi-Fi on this device", U""},
                              ListItem{U"Log in on this device", U""}};
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
  buttons_ = ButtonPad::active();
  pad_.reset();
}

void SetupScreen::activate(int i) {
  if (i == 0) router::go(router::Id::Wifi, "setup");
  else if (i == 1) router::go(router::Id::LoginForm);
  else if (i == 2 && hasRestore_) picker_.open();
}

void SetupScreen::onKey(const KeyEvent& k) {
  if (picker_.isOpen()) return picker_.onKey(k);
  if (buttons_) {
    const int i = pad_.key(k);
    if (i < 0) return;
    activate(i);
    return;
  }
  switch (k.action) {
    case Action::Up:
      menu_.up();
      break;
    case Action::Down:
      menu_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && menu_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right:
      activate(menu_.selected());
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
  auto line = [&](int col, const std::string& text, uint16_t fg) {
    if (narrowGrid(g)) r = putWrapped(g, col, r, text, fg, t.bg);
    else putLine(g, col, r++, text, fg, t.bg);
  };
  if (wifimgr::mode() == wifimgr::Mode::Setup) {
    line(1, "Join this Wi-Fi network:", t.fg);
    line(3, wifimgr::apSsid(), t.selectBg);
    line(3, wifimgr::apHasPassword() ? "password required" : "open network", t.dim);
    line(1, "then browse to:", t.fg);
    line(3, "http://192.168.4.1", t.selectBg);
    ++r;
  }
  if (wifiUp()) {
    line(1, "On " + wifiSsid() + ", browse to:", t.fg);
    line(3, "http://monster-cyberspace.local", t.selectBg);
    line(3, "http://" + wifiIp(), t.selectBg);
    ++r;
  }
  if (!app().cfg.wifi.empty() && app().auth != 1 && app().auth != 2) {
    line(1, "Sign in at monster-cyberspace.local", t.fg);
    line(1, "or on this device.", t.fg);
  }
  int bottom = g.rows() - 1;
  const int n = int(menu_.size());
  if (buttons_) {
    // Side by side where the shorter labels fit, else stacked; drawn last so
    // they stay whole on a short screen.
    std::vector<ListItem> items{ListItem{U"Join Wi-Fi", U""}, ListItem{U"Log in on device", U""}};
    if (hasRestore_) items.push_back(ListItem{U"Restore from SD", U""});
    int columns = n;
    while (columns > 1 && (g.cols() - 1) / columns - 3 < 16) --columns;
    const int rows = (n + columns - 1) / columns;
    const int top = std::max(1, bottom - rows * (kButtonRows + 1));
    for (int row = top; row < bottom; ++row) g.fillRow(row, t.bg);
    pad_.render(g, t, items, top, bottom, false, columns);
    drawHint(g, t, U"tap to select");
    return;
  }
  menu_.render(g, bottom - n, bottom, 1, g.cols() - 2, t);
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF select", U"tap to select");
}

}  // namespace rc
