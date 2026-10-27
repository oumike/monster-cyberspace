#include <Arduino.h>

#include "app/app_state.h"
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "store/settings.h"
#include "tz/zones.h"
#include "ui/rotation.h"

namespace rc {

namespace {
enum Item { kTheme, kTextSize, kZone, kRotation, kSetupAp, kBackup, kRestore, kLogout, kReset };

// The visible menu, in order. Orientation appears only on a board that can
// rotate and the SD-card items only on a board with a slot, so item indices and
// the menu height follow from this.
std::vector<Item> menuItems() {
  std::vector<Item> v{kTheme, kTextSize, kZone};
  if (board::canRotate()) v.push_back(kRotation);
  v.push_back(kSetupAp);
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
      if (confirmBox_.selected() != 1) break;
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
      switch (menuItems()[menu_.selected()]) {
        case kTheme: {
          AppState& a = app();
          a.cfg.theme = nextTheme(a.cfg.theme);
          settings::saveUiPrefs(a.cfg);
          break;
        }
        case kTextSize:
          ui::cycleTextSize();
          break;
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
        case kSetupAp: {
          Command c{CmdType::SetupAp};
          c.n = setupApUp() ? 0 : 1;
          chatSource().command(c);
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
      break;
    default:
      break;
  }
}

void SettingsScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"settings", -1);
  if (picker_.isOpen()) return picker_.render(g, t);
  const AppState& a = app();
  int r = 1;
  const int bottom = g.rows() - 1;
  const std::vector<Item> items = menuItems();
  const int nItems = int(items.size());
  // Status lines stop above the menu.
  // On a narrow grid a value too long for its row continues on the next rows.
  const bool narrow = narrowGrid(g);
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    if (r >= bottom - nItems) return;
    putLine(g, 1, r, label, t.dim, t.bg);
    if (!narrow) {
      putLine(g, 11, r++, value, fg, t.bg);
      return;
    }
    for (const std::u32string& line : wrapText(utf8ToU32(value), g.cols() - 11, g.cols() - 11)) {
      if (r >= bottom - nItems) break;
      g.putText(11, r++, line, fg, t.bg, g.cols() - 11);
    }
  };
  field("account", a.user.empty() ? "not signed in" : "@" + a.user, t.fg);
  field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
  field("config", "http://platy-cyberspace.local", t.fg);
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

  menu_.setItems([&] {
    std::vector<ListItem> rows;
    for (Item it : items) {
      switch (it) {
        case kTheme:
          rows.push_back(ListItem{U"Theme: " + widen(themeLabel(a.cfg.theme)), U""});
          break;
        case kTextSize:
          rows.push_back(ListItem{U"Text size: " + textSizeLabel(a.cfg.textSize), U""});
          break;
        case kZone:
          rows.push_back(ListItem{U"Time zone: " + widen(zoneLabel(ts.tzName)), U""});
          break;
        case kRotation:
          rows.push_back(ListItem{U"Orientation: " + widen(rotationLabel(a.cfg.rotation)), U""});
          break;
        case kSetupAp:
          rows.push_back(ListItem{ap ? U"Turn setup AP off" : U"Turn setup AP on", U""});
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
    return rows;
  }());
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
    // The orientation picker is a centred box with all four; the bottom
    // strip only has room for a two-row confirm.
    confirmBox_.render(g, 1, bottom, 0,
                       confirm_ == Confirm::Rotation ? OverlayPlace::Centre : OverlayPlace::Bottom, t);
    if (confirm_ == Confirm::Rotation)
      drawHint(g, t, U"\u2191\u2193 pick  \u25CF set  back: cancel",
               U"tap: select  outside: cancel");
    else
      drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel",
               U"tap: confirm  outside: cancel");
  } else {
    menu_.render(g, bottom - nItems, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF select  back: home", U"tap to select");
  }
}

}  // namespace rc
