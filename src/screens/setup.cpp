#include <algorithm>
#include <utility>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "feature_flags.h"
#include "hal/board.h"
#include "net/wifi_mgr.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "util/utf8.h"
#include "ui/button_grid.h"
#include "web/web_server.h"

namespace rc {

namespace {

bool pageUp() {
#if RC_WEB_OPT_IN
  return web::running();
#else
  return false;
#endif
}

}  // namespace

void SetupScreen::enter(const std::string&) {
  options_.clear();
  if (RC_FEAT_WIFI_MGMT) options_.push_back(Option::JoinWifi);
  options_.push_back(Option::LogIn);
  // Web config as an opt-in (Linux): turned on here too, to sign in from a browser.
  if (RC_WEB_OPT_IN) options_.push_back(Option::WebConfig);
  hasRestore_ = false;
  if (board::hasSdSlot()) {
    std::vector<backupstore::FileInfo> files;
    const backupstore::Result r = backupstore::listSd(files);
    if (r.ok && !files.empty()) {
      options_.push_back(Option::Restore);
      hasRestore_ = true;
    }
  }
  std::vector<ListItem> items;
  for (const Option o : options_) {
    switch (o) {
      case Option::JoinWifi: items.push_back(ListItem{U"Join Wi-Fi on this device", U""}); break;
      case Option::LogIn: items.push_back(ListItem{U"Log in on this device", U""}); break;
      case Option::WebConfig: items.push_back(ListItem{U"Web config page", U""}); break;
      case Option::Restore:
        items.push_back(ListItem{U"Restore from " + utf8ToU32(board::backupPlaceName()), U""});
        break;
    }
  }
  menu_.setItems(std::move(items));
  menu_.setSelected(0);
  buttons_ = ButtonPad::active();
  pad_.reset();
}

void SetupScreen::activate(int i) {
  if (i < 0 || size_t(i) >= options_.size()) return;
  const Option o = options_[size_t(i)];
  if ((o == Option::JoinWifi || o == Option::LogIn) &&
      ui::typingElsewhere(RC_FEAT_WIFI_MGMT ? "Join Wi-Fi and sign in" : "Sign in"))
    return;
  switch (o) {
    case Option::JoinWifi:
#if RC_FEAT_WIFI_MGMT
      router::go(router::Id::Wifi, "setup");
#endif
      break;
    case Option::LogIn: router::go(router::Id::LoginForm); break;
    case Option::WebConfig:
#if RC_WEB_OPT_IN
      router::goFrom(router::Id::Setup, router::Id::WebConfig);
#endif
      break;
    case Option::Restore: picker_.open(); break;
  }
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
  if (v_->gate_.due(v_->text_.obj(), nowMs, {wifiUp(), setupApUp(), a.auth, a.cfg.wifi.empty(), pageUp()}))
    sync();
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
#if !RC_FAKE_API && RC_FEAT_WIFI_MGMT
  if (wifimgr::mode() == wifimgr::Mode::Setup) {
    line(1, "Join this Wi-Fi network:", t.fg);
    line(3, wifimgr::apSsid(), t.selectBg);
    line(3, wifimgr::apHasPassword() ? "password required" : "open network", t.dim);
    line(1, "then browse to:", t.fg);
    line(3, "http://192.168.4.1", t.selectBg);
    ++r;
  }
#endif
  const bool signingIn = app().auth != 1 && app().auth != 2;
#if RC_WEB_OPT_IN
  // The page is served only once turned on, on web::kPort, and with no mDNS name.
  if (wifiUp() && pageUp()) {
    const std::string ssid = wifiSsid();
    line(1, ssid.empty() ? "Browse to:" : "On " + ssid + ", browse to:", t.fg);
    line(3, "http://" + wifiIp() + ":" + std::to_string(web::kPort), t.selectBg);
    ++r;
    if (signingIn) line(1, "Sign in there or on this device.", t.fg);
  } else if (wifiUp() && signingIn) {
    line(1, "Sign in on this device, or turn", t.fg);
    line(1, "on the web config page to use", t.fg);
    line(1, "a browser.", t.fg);
  }
#else
  if (wifiUp()) {
    line(1, "On " + wifiSsid() + ", browse to:", t.fg);
    line(3, "http://monster-cyberspace.local", t.selectBg);
    line(3, "http://" + wifiIp(), t.selectBg);
    ++r;
  }
  const bool online = RC_FEAT_WIFI_MGMT ? !app().cfg.wifi.empty() : wifiUp();
  if (online && signingIn) {
    line(1, "Sign in at monster-cyberspace.local", t.fg);
    line(1, "or on this device.", t.fg);
  }
#endif
  v_->text_.end();
  if (buttons_) {
    std::vector<ListItem> items;
    for (const Option o : options_) {
      switch (o) {
        case Option::JoinWifi: items.push_back(ListItem{U"Join Wi-Fi", U""}); break;
        case Option::LogIn: items.push_back(ListItem{U"Log in on device", U""}); break;
        case Option::WebConfig: items.push_back(ListItem{U"Web config", U""}); break;
        case Option::Restore:
          items.push_back(ListItem{board::backupOnCard() ? U"Restore from SD" : U"Restore backup", U""});
          break;
      }
    }
    pad_.sync(v_->padView_, items, top, bottom, false, columns);
    syncHint(v_->hint_, U"tap to select");
    return;
  }
  placeCells(v_->menuView_.obj(), 1, top, v_->text_.cols() - 2, bottom);
  v_->menuView_.sync(menu_);
  syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF select", U"tap to select");
}

}  // namespace rc
