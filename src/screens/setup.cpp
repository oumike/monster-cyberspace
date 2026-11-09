#include <algorithm>
#include <utility>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "screens/lv_draw.h"
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
  if (i < 2 && ui::typingElsewhere("Join Wi-Fi and sign in")) return;
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

void SetupScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->text_.create(root);
  v_->menuView_.create(root);
  v_->padView_.create(root);
  v_->hint_.create(root);
  picker_.build(root);
  v_->status_.create(root);
  sync();
}

void SetupScreen::update(int64_t nowMs) {
  const AppState& a = app();
  if (v_->gate_.due(v_->text_.obj(), nowMs, {wifiUp(), setupApUp(), a.auth, a.cfg.wifi.empty()})) sync();
}

void SetupScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(v_->status_, U"setup", -1);
  picker_.sync();
  const bool picking = picker_.isOpen();
  lvui::setHidden(v_->text_.obj(), picking);
  lvui::setHidden(v_->hint_.obj(), picking);
  lvui::setHidden(v_->menuView_.obj(), picking || buttons_);
  lvui::setHidden(v_->padView_.obj(), picking || !buttons_);
  if (picking) return;
  const int bottom = v_->text_.rows() - 1;
  const int n = int(menu_.size());
  // The options go over the text's last rows, as the grid draws them last.
  int columns = n, top = bottom - n;
  if (buttons_) {
    while (columns > 1 && (v_->text_.cols() - 1) / columns - 3 < 16) --columns;
    top = std::max(1, bottom - (n + columns - 1) / columns * (kButtonRows + 1));
  }
  int r = 2;
  const bool narrow = narrowGrid(v_->text_);
  v_->text_.begin();
  auto line = [&](int col, const std::string& s, uint16_t fg) {
    if (narrow) r = v_->text_.wrapped(col, r, s, fg, top);
    else if (r < top) v_->text_.line(col, r++, s, fg);
    else ++r;
  };
#if !RC_FAKE_API
  if (wifimgr::mode() == wifimgr::Mode::Setup) {
    line(1, "Join this Wi-Fi network:", t.fg);
    line(3, wifimgr::apSsid(), t.selectBg);
    line(3, wifimgr::apHasPassword() ? "password required" : "open network", t.dim);
    line(1, "then browse to:", t.fg);
    line(3, "http://192.168.4.1", t.selectBg);
    ++r;
  }
#endif
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
  v_->text_.end();
  if (buttons_) {
    std::vector<ListItem> items{ListItem{U"Join Wi-Fi", U""}, ListItem{U"Log in on device", U""}};
    if (hasRestore_) items.push_back(ListItem{U"Restore from SD", U""});
    pad_.sync(v_->padView_, items, top, bottom, false, columns);
    syncHint(v_->hint_, U"tap to select");
    return;
  }
  placeCells(v_->menuView_.obj(), 1, top, v_->text_.cols() - 2, bottom);
  v_->menuView_.sync(menu_);
  syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF select", U"tap to select");
}

}  // namespace rc
