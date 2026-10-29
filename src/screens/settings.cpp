#include <algorithm>
#include <Arduino.h>

#include "app/app_state.h"
#include "app/ota_service.h"
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "core_version.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "store/settings.h"
#include "tz/zones.h"
#include "ui/rotation.h"
#include "ui/kb_light.h"
#include "ui/screen_timeout.h"

namespace rc {

namespace {
// The keyboard light picker's order.
constexpr KbLight kKbLightOrder[] = {KbLight::Low, KbLight::Medium, KbLight::High, KbLight::Off};

enum Item {
  kTheme,
  kTextSize,
  kZone,
  kRotation,
  kScreenTimeout,
  kAtTimeout,
  kKbLight,
  kSpellCheck,
  kWifi,
  kSetupAp,
  kFirmware,
  kAutoUpdate,
  kBackup,
  kRestore,
  kLogout,
  kReset
};

// The visible menu, in order. Theme is hidden on e-paper (one theme there),
// orientation appears only on a board that can rotate and the SD-card items
// only on a board with a slot, so item indices and the menu height follow
// from this.
std::vector<Item> menuItems() {
  std::vector<Item> v;
  if (!board::eink()) v.push_back(kTheme);
  v.push_back(kTextSize);
  v.push_back(kZone);
  if (board::canRotate()) v.push_back(kRotation);
  v.push_back(kScreenTimeout);
  v.push_back(kAtTimeout);
  if (board::hasKbLight()) v.push_back(kKbLight);
  v.push_back(kSpellCheck);
  v.push_back(kWifi);
  v.push_back(kSetupAp);
  v.push_back(kFirmware);
  v.push_back(kAutoUpdate);
  if (board::hasSdSlot()) {
    v.push_back(kBackup);
    v.push_back(kRestore);
  }
  v.push_back(kLogout);
  v.push_back(kReset);
  return v;
}

std::u32string widen(const char* s) {
  std::u32string out;
  for (; *s; ++s) out += static_cast<char32_t>(static_cast<unsigned char>(*s));
  return out;
}

// The Firmware item's value: the running version, or what an update is doing.
std::u32string firmwareValue(const ota::Status& s) {
  switch (s.state) {
    case ota::State::Checking:
      return U"checking\u2026";
    case ota::State::UpToDate:
      return widen("v" RC_VERSION " up to date");
    case ota::State::Available:
      return widen((s.latest + " available").c_str());
    case ota::State::Installing: {
      const int p = ota::percent(s);
      return p < 0 ? U"installing\u2026" : widen(("installing " + std::to_string(p) + "%").c_str());
    }
    case ota::State::Done:
      return U"restarting\u2026";
    case ota::State::Failed:
      return U"update failed";
    case ota::State::Idle:
      break;
  }
  return widen("v" RC_VERSION);
}

// The text size picker's rows, in size order.
constexpr TextSize kSizeOrder[] = {TextSize::Compact, TextSize::Comfortable, TextSize::Large};
}  // namespace

void SettingsScreen::enter(const std::string&) {
  confirm_ = Confirm::None;
  picking_ = false;
  menu_.setSelected(0);
  pad_.reset();
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
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && zoneList_.select(k.index)) zoneKey(KeyEvent{k.then});
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

void SettingsScreen::confirmKey(const KeyEvent& k) {
  const Confirm c = confirm_;
  switch (confirmBox_.key(k)) {
    case Overlay::Result::None:
      break;
    case Overlay::Result::Closed:
      confirm_ = Confirm::None;
      break;
    case Overlay::Result::Picked:
      confirm_ = Confirm::None;
      if (c == Confirm::Rotation) {
        const int i = confirmBox_.selected();
        if (i >= 0 && i < 4) ui::setRotation(kRotationOrder[i]);
        break;
      }
      if (c == Confirm::Timeout) {
        const int i = confirmBox_.selected();
        if (i >= 0 && i < int(std::size(kScreenTimeoutSecs))) {
          app().cfg.screenOnSecs = kScreenTimeoutSecs[i];
          settings::saveScreen(app().cfg.screenOnSecs, app().cfg.screenSaver);
        }
        break;
      }
      if (c == Confirm::Theme) {
        const int i = confirmBox_.selected();
        if (i >= 0 && i < kThemeCount) {
          app().cfg.theme = themeFromIndex(i);
          settings::saveUiPrefs(app().cfg);
        }
        break;
      }
      if (c == Confirm::KbLight) {
        const int i = confirmBox_.selected();
        if (i >= 0 && i < int(std::size(kKbLightOrder))) {
          app().cfg.kbLight = kKbLightOrder[i];
          settings::saveKbLight(app().cfg.kbLight);
        }
        break;
      }
      if (c == Confirm::TextSize) {
        const int i = confirmBox_.selected();
        if (i >= 0 && i < 3 && kSizeOrder[i] != app().cfg.textSize) ui::setTextSize(kSizeOrder[i]);
        break;
      }
      if (confirmBox_.selected() != 1) break;
      if (c == Confirm::Update) {
        if (!ota::startInstall()) ui::banner("Can't update now: is Wi-Fi up?", true);
        break;
      }
      if (c == Confirm::Logout) {
        chatSource().command(Command{CmdType::Logout});
      } else {
        settings::factoryReset();
        delay(100);
        ESP.restart();
      }
      break;
  }
}

void SettingsScreen::onKey(const KeyEvent& k) {
  if (picker_.isOpen()) return picker_.onKey(k);
  if (picking_) return zoneKey(k);
  if (confirm_ != Confirm::None) return confirmKey(k);
  if (buttons_ && k.action != Action::Back && k.action != Action::Left) {
    const std::vector<Item> items = menuItems();
    const int i = pad_.key(k);
    if (i >= 0 && i < int(items.size())) activate(items[i]);
    return;
  }
  switch (k.action) {
    case Action::Up:
      menu_.up();
      break;
    case Action::Down:
      menu_.down();
      break;
    case Action::Back:
    case Action::Left:
      router::go(router::Id::Home);
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && menu_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right:
      activate(menuItems()[menu_.selected()]);
      break;
    default:
      break;
  }
}

void SettingsScreen::activate(int item) {
  switch (item) {
    case kTheme: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (int i = 0; i < kThemeCount; ++i) {
        ListItem it{widen(themeLabel(themeFromIndex(i))), U""};
        if (themeFromIndex(i) == app().cfg.theme) {
          it.marker = true;
          sel = i;
        }
        rows.push_back(std::move(it));
      }
      confirm_ = Confirm::Theme;
      confirmBox_.open(U"theme", std::move(rows), sel);
      break;
    }
    case kTextSize: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (int i = 0; i < 3; ++i) {
        ListItem it{textSizeLabel(kSizeOrder[i]), U""};
        if (kSizeOrder[i] == app().cfg.textSize) {
          it.marker = true;
          sel = i;
        }
        rows.push_back(std::move(it));
      }
      confirm_ = Confirm::TextSize;
      confirmBox_.open(U"text size", std::move(rows), sel);
      break;
    }
    case kZone:
      openZones();
      break;
    case kRotation: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (int i = 0; i < 4; ++i) {
        ListItem it{widen(rotationLabel(kRotationOrder[i])), U""};
        if (kRotationOrder[i] == app().cfg.rotation) {
          it.marker = true;
          sel = i;
        }
        rows.push_back(std::move(it));
      }
      confirm_ = Confirm::Rotation;
      confirmBox_.open(U"orientation", std::move(rows), sel);
      break;
    }
    case kScreenTimeout: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (size_t i = 0; i < std::size(kScreenTimeoutSecs); ++i) {
        ListItem it{widen(screenTimeoutLabel(kScreenTimeoutSecs[i]).c_str()), U""};
        if (kScreenTimeoutSecs[i] == app().cfg.screenOnSecs) {
          it.marker = true;
          sel = int(i);
        }
        rows.push_back(std::move(it));
      }
      confirm_ = Confirm::Timeout;
      confirmBox_.open(U"screen timeout", std::move(rows), sel);
      break;
    }
    case kKbLight: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (size_t i = 0; i < std::size(kKbLightOrder); ++i) {
        ListItem it{kbLightLabel(kKbLightOrder[i]), U""};
        if (kKbLightOrder[i] == app().cfg.kbLight) {
          it.marker = true;
          sel = int(i);
        }
        rows.push_back(std::move(it));
      }
      confirm_ = Confirm::KbLight;
      confirmBox_.open(U"keyboard light", std::move(rows), sel);
      break;
    }
    case kAtTimeout: {
      AppState& a = app();
      a.cfg.screenSaver = !a.cfg.screenSaver;
      settings::saveScreen(a.cfg.screenOnSecs, a.cfg.screenSaver);
      break;
    }
    case kSpellCheck:
      app().cfg.spellCheck = !app().cfg.spellCheck;
      settings::saveSpellCheck(app().cfg.spellCheck);
      break;
    case kWifi:
      router::go(router::Id::Wifi);
      break;
    case kSetupAp: {
      Command c{CmdType::SetupAp};
      c.n = setupApUp() ? 0 : 1;
      chatSource().command(c);
      break;
    }
    case kFirmware: {
      const ota::Status st = ota::status();
      if (!st.layoutOk) {
        ui::banner("Updates need a USB install of a release first", true);
        break;
      }
      if (st.state == ota::State::Checking || st.state == ota::State::Installing ||
          st.state == ota::State::Done)
        break;
      if (st.state == ota::State::Available) {
        confirm_ = Confirm::Update;
        confirmBox_.confirm(widen(("Install " + st.latest + "? The device restarts.").c_str()), U"Install");
        break;
      }
      if (!ota::startCheck()) ui::banner("Can't check now: is Wi-Fi up?", true);
      break;
    }
    case kAutoUpdate: {
      AppState& a = app();
      a.cfg.autoUpdate = !a.cfg.autoUpdate;
      settings::saveAutoUpdate(a.cfg.autoUpdate);
      break;
    }
    case kBackup: {
      const backupstore::Result r = backupstore::saveToSd();
      ui::banner(r.message, !r.ok);
      break;
    }
    case kRestore:
      picker_.open();
      break;
    case kLogout:
      confirm_ = Confirm::Logout;
      confirmBox_.confirm(U"Log out of Cyberspace?", U"Log out");
      break;
    case kReset:
      confirm_ = Confirm::Reset;
      confirmBox_.confirm(U"Erase Wi-Fi, account and settings?", U"Erase everything");
      break;
  }
}

void SettingsScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"settings", -1);
  if (picker_.isOpen()) return picker_.render(g, t);
  const AppState& a = app();
  const int bottom = g.rows() - 1;
  const std::vector<Item> items = menuItems();
  const int nItems = int(items.size());
  const bool ap = setupApUp();
  const timesvc::Snapshot ts = timesvc::snapshot();
  buttons_ = ButtonPad::active();

  // Each item as a label and its current value; lines join them, buttons put
  // the value at the right.
  std::vector<ListItem> rows;
  for (Item it : items) {
    switch (it) {
      case kTheme:
        rows.push_back(ListItem{U"Theme", widen(themeLabel(a.cfg.theme))});
        break;
      case kTextSize:
        rows.push_back(ListItem{U"Text size", textSizeLabel(a.cfg.textSize)});
        break;
      case kZone:
        rows.push_back(ListItem{U"Time zone", widen(zoneLabel(ts.tzName))});
        break;
      case kRotation:
        rows.push_back(ListItem{U"Orientation", widen(rotationLabel(a.cfg.rotation))});
        break;
      case kScreenTimeout:
        rows.push_back(ListItem{U"Screen timeout", widen(screenTimeoutLabel(a.cfg.screenOnSecs).c_str())});
        break;
      case kKbLight:
        rows.push_back(ListItem{U"Keyboard light", kbLightLabel(a.cfg.kbLight)});
        break;
      case kAtTimeout:
        rows.push_back(ListItem{U"At timeout", a.cfg.screenSaver ? U"screensaver" : U"screen off"});
        break;
      case kSpellCheck:
        rows.push_back(ListItem{U"Spell check", a.cfg.spellCheck ? U"on" : U"off"});
        break;
      case kWifi:
        rows.push_back(ListItem{U"Wi-Fi", wifiUp() ? utf8ToU32(wifiSsid()) : U"not connected"});
        break;
      case kSetupAp:
        if (buttons_)
          rows.push_back(ListItem{U"Setup AP", ap ? U"on" : U"off", ItemTone::Normal, ap});
        else
          rows.push_back(ListItem{ap ? U"Turn setup AP off" : U"Turn setup AP on", U""});
        break;
      case kFirmware: {
        const ota::Status st = ota::status();
        rows.push_back(ListItem{U"Firmware", firmwareValue(st),
                                st.state == ota::State::Failed ? ItemTone::Error : ItemTone::Normal,
                                st.state == ota::State::Available});
        break;
      }
      case kAutoUpdate:
        rows.push_back(ListItem{U"Auto-update", a.cfg.autoUpdate ? U"on" : U"off", ItemTone::Normal,
                                a.cfg.autoUpdate});
        break;
      case kBackup:
        rows.push_back(ListItem{U"Back up to SD card", U""});
        break;
      case kRestore:
        rows.push_back(ListItem{U"Restore from SD card\u2026", U""});
        break;
      case kLogout:
        rows.push_back(ListItem{U"Log out", U""});
        break;
      case kReset:
        rows.push_back(ListItem{U"Factory reset\u2026", U"", ItemTone::Error});
        break;
    }
  }

  // The menu sits at the bottom and scrolls, so the account, Wi-Fi and
  // config lines above it always have room.
  constexpr int kStatusRows = 3;
  const int menuRows = std::clamp(bottom - 1 - kStatusRows, std::min(nItems, 4), nItems);
  const int menuTop = bottom - menuRows;
  if (buttons_) {
    // Touch-only boards: the menu as buttons, without the status lines.
    if (!picking_) pad_.render(g, t, rows, 2, bottom, true);
  } else {
    int r = 1;
    // Status lines stop above the menu.
    // On a narrow grid a value too long for its row continues on the next rows.
    const bool narrow = narrowGrid(g);
    auto field = [&](const char* label, const std::string& value, uint16_t fg) {
      if (r >= menuTop) return;
      putLine(g, 1, r, label, t.dim, t.bg);
      if (!narrow) {
        putLine(g, 11, r++, value, fg, t.bg);
        return;
      }
      for (const std::u32string& line : wrapText(utf8ToU32(value), g.cols() - 11, g.cols() - 11)) {
        if (r >= menuTop) break;
        g.putText(11, r++, line, fg, t.bg, g.cols() - 11);
      }
    };
    field("account", a.user.empty() ? "not signed in" : "@" + a.user, t.fg);
    field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
    field("config", "http://monster-cyberspace.local", t.fg);
    int bat = board::batteryPercent();
    field("battery", bat < 0 ? "unknown" : std::to_string(bat) + "%", t.fg);
    field("setup AP", ap ? "on" : "off", ap ? t.selectBg : t.fg);
    if (ap) {
      field("", setupApSsid(), t.selectBg);
      field("", setupApHasPassword() ? "password required" : "open network", t.dim);
      field("", "http://192.168.4.1", t.selectBg);
    }
    field("gps", ts.gps, t.fg);
    field("time", ts.time, t.fg);
    field("zone", ts.zone, t.fg);
    for (ListItem& it : rows) {
      if (!it.right.empty()) it.label += U": " + it.right;
      it.right.clear();
    }
    menu_.setItems(std::move(rows));
  }

  if (picking_) {
    // Drawn over the status lines and the menu, below the status bar.
    const int top = 1;
    for (int c = 0; c < g.cols(); ++c) g.put(c, top, U' ', t.barFg, t.barBg);
    g.putText(1, top, U"time zone", t.barFg, t.barBg, g.cols() - 2);
    for (int row = top + 1; row < bottom; ++row)
      for (int c = 0; c < g.cols(); ++c) g.put(c, row, U' ', t.fg, t.bg);
    zoneList_.render(g, top + 1, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF set  back: cancel", U"tap to select");
  } else if (confirm_ != Confirm::None) {
    // The orientation and text size pickers are centred boxes; the bottom
    // strip only has room for a two-row confirm.
    const bool pick = confirm_ == Confirm::Rotation || confirm_ == Confirm::TextSize ||
                      confirm_ == Confirm::Timeout || confirm_ == Confirm::KbLight ||
                      confirm_ == Confirm::Theme;
    confirmBox_.render(g, 1, bottom, 0, pick ? OverlayPlace::Centre : OverlayPlace::Bottom, t);
    if (pick)
      drawHint(g, t, U"\u2191\u2193 pick  \u25CF set  back: cancel",
               U"tap: select  outside: cancel");
    else
      drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel",
               U"tap: confirm  outside: cancel");
  } else if (buttons_) {
    drawHint(g, t, U"", U"tap to select  drag to scroll");
  } else {
    menu_.render(g, menuTop, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF select  back: home", U"tap to select");
  }
}

}  // namespace rc
