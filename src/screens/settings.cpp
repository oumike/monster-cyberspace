#include <algorithm>
#include <Arduino.h>

#include "app/app_state.h"
#include "feature_flags.h"
#if RC_FEAT_OTA
#include "app/ota_service.h"
#endif
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "core_version.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "store/settings.h"
#include "power/power_policy.h"
#include "tz/zones.h"
#include "ui/rotation.h"
#include "ui/kb_light.h"
#include "ui/notify_light.h"
#include "ui/screen_timeout.h"

namespace rc {

namespace {
// The notify light picker's order.
constexpr LedColor kLedColorOrder[] = {LedColor::Red,  LedColor::Green,   LedColor::Blue,
                                       LedColor::Yellow, LedColor::Cyan, LedColor::Magenta,
                                       LedColor::White, LedColor::Off};

std::u32string volumeLabel(uint8_t pct) {
  std::u32string s;
  for (char ch : std::to_string(pct)) s += char32_t(ch);
  return s + U"%";
}

enum Item {
  kTheme,
  kTextSize,
  kZone,
  kRotation,
  kScreenTimeout,
  kAtTimeout,
  kKbLight,
  kSound,
  kTapVibrate,
  kAlertVibrate,
  kNotifyLight,
  kNotifyTimeout,
  kSpellCheck,
  kIdle,
  kWake,
  kWifi,
  kWebConfig,
  kSetupAp,
  kFirmware,
  kAutoUpdate,
  kBackup,
  kRestore,
  kLogout,
  kReset
};

// The visible menu, in order. Theme is hidden on e-paper (one theme there),
// text size on a board with one size, orientation appears only on a board
// that can rotate and the SD-card items
// only on a board with a slot, so item indices and the menu height follow
// from this. Spell check and Auto-update go when their features are compiled
// out; Firmware then just shows the version.
std::vector<Item> menuItems() {
  std::vector<Item> v;
  if (!board::eink()) v.push_back(kTheme);
  if (TextSize only; !board::fixedTextSize(only)) v.push_back(kTextSize);
  v.push_back(kZone);
  if (board::canRotate()) v.push_back(kRotation);
  v.push_back(kScreenTimeout);
  v.push_back(kAtTimeout);
  if (board::hasKbLight()) v.push_back(kKbLight);
  if (board::hasSound()) v.push_back(kSound);
  if (board::hasHaptic()) {
    v.push_back(kTapVibrate);
    v.push_back(kAlertVibrate);
  }
  if (board::hasNotifyLight()) {
    v.push_back(kNotifyLight);
    v.push_back(kNotifyTimeout);
  }
  if (RC_FEAT_SPELL && board::canType()) v.push_back(kSpellCheck);
  if (board::sleepsOnBattery()) {
    v.push_back(kIdle);
    v.push_back(kWake);
  }
  v.push_back(kWifi);
  v.push_back(kWebConfig);
  v.push_back(kSetupAp);
  v.push_back(kFirmware);
  if (RC_FEAT_OTA) v.push_back(kAutoUpdate);
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

#if RC_FEAT_OTA
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
#endif

std::u32string idleLabel(uint32_t secs) {
  return widen((std::to_string(secs / 60) + " min").c_str());
}

std::u32string wakeLabel(uint32_t mins) {
  if (mins == 0) return U"never";
  if (mins >= 60) return widen((std::to_string(mins / 60) + " h").c_str());
  return widen((std::to_string(mins) + " min").c_str());
}

#ifndef RC_LOW_MEM
#define RC_LOW_MEM 0
#endif

// The text size picker's rows, in size order.
constexpr TextSize kSizeOrder[] = {TextSize::Compact, TextSize::Comfortable, TextSize::Large};
// Low-memory boards (no PSRAM) offer no Compact: its grid does not fit.
constexpr int kFirstSize = RC_LOW_MEM ? 1 : 0;
}  // namespace

void SettingsScreen::enter(const std::string&) {
  confirm_ = Confirm::None;
  closeVolume();
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
  showChoices(U"time zone", items, sel);
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
    case Action::Right:
      picking_ = false;
      applyZone(zoneList_.selected());
      break;
    default:
      break;
  }
}

void SettingsScreen::applyZone(int i) {
  if (i < 0 || size_t(i) >= zoneCount()) return;
  const Zone& z = zoneAt(size_t(i));
  settings::saveTz(z.posix, z.iana);
  timesvc::settingsChanged();
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
      applyChoice(c, confirmBox_.selected());
      break;
  }
}

void SettingsScreen::applyChoice(Confirm c, int i) {
  if (c == Confirm::Rotation) {
    if (i >= 0 && i < 4) ui::setRotation(kRotationOrder[i]);
    return;
  }
  if (c == Confirm::Timeout) {
    if (i >= 0 && i < int(std::size(kScreenTimeoutSecs))) {
      app().cfg.screenOnSecs = kScreenTimeoutSecs[i];
      settings::saveScreen(app().cfg.screenOnSecs, app().cfg.screenSaver);
    }
    return;
  }
  if (c == Confirm::Theme) {
    if (i >= 0 && i < kThemeCount) {
      app().cfg.theme = themeFromIndex(i);
      settings::saveUiPrefs(app().cfg);
    }
    return;
  }
  if (c == Confirm::KbLight) {
    if (i >= 0 && i < int(std::size(kKbLightOrder))) {
      app().cfg.kbLight = kKbLightOrder[i];
      settings::saveKbLight(app().cfg.kbLight);
    }
    return;
  }
  if (c == Confirm::NotifyLight) {
    if (i >= 0 && i < int(std::size(kLedColorOrder))) {
      app().cfg.notifyLight = kLedColorOrder[i];
      settings::saveNotifyLight(app().cfg.notifyLight);
      ui::notifyLightPreview(app().cfg.notifyLight);
    }
    return;
  }
  if (c == Confirm::NotifyTimeout) {
    if (i >= 0 && i < kNotifyTimeoutCount) {
      app().cfg.notifyTimeoutSecs = kNotifyTimeoutSecs[i];
      settings::saveNotifyTimeout(app().cfg.notifyTimeoutSecs);
    }
    return;
  }
  if (c == Confirm::Idle || c == Confirm::Wake) {
    AppState& a = app();
    if (c == Confirm::Idle && i >= 0 && i < int(std::size(kIdleChoices)))
      a.cfg.idleSecs = kIdleChoices[i];
    else if (c == Confirm::Wake && i >= 0 && i < int(std::size(kWakeChoices)))
      a.cfg.wakeMins = kWakeChoices[i];
    else
      return;
    settings::savePower(a.cfg.idleSecs, a.cfg.wakeMins);
    return;
  }
  if (c == Confirm::TextSize) {
    const int s = i + kFirstSize;
    if (s >= kFirstSize && s < 3 && kSizeOrder[s] != app().cfg.textSize) ui::setTextSize(kSizeOrder[s]);
    return;
  }
  if (i != 1) return;
#if RC_FEAT_OTA
  if (c == Confirm::Update) {
    if (!ota::startInstall()) ui::banner("Can't update now: is Wi-Fi up?", true);
    return;
  }
#endif
  if (c == Confirm::Logout) {
    chatSource().command(Command{CmdType::Logout});
  } else {
    settings::factoryReset();
    delay(100);
    ESP.restart();
  }
}

void SettingsScreen::openChoice(Confirm c, std::u32string title, std::vector<ListItem> rows, int sel) {
  confirm_ = c;
  showChoices(title, rows, sel);
  confirmBox_.open(std::move(title), std::move(rows), sel);
}

void SettingsScreen::openConfirm(Confirm c, std::u32string title, std::u32string action) {
  confirm_ = c;
  std::vector<ListItem> rows{ListItem{U"Cancel", U""}, ListItem{action, U"", ItemTone::Error}};
  showChoices(title, std::move(rows), 0);
  confirmBox_.confirm(std::move(title), std::move(action));
}

void SettingsScreen::showChoices(std::u32string title, std::vector<ListItem> rows, int sel) {
  choiceTitle_ = std::move(title);
  choices_ = std::move(rows);
  choicePad_.reset();
  if (!ui::fromTouch()) choicePad_.showCursorAt(sel);
}

void SettingsScreen::closeChoice() {
  confirm_ = Confirm::None;
  picking_ = false;
  confirmBox_.close();
}

// Touch boards: a choice menu, the time zone list or a confirm as buttons.
void SettingsScreen::choiceKey(const KeyEvent& k) {
  const bool leave = k.action == Action::Back ||
                     (k.action == Action::Left && (!board::hasKeys() || choicePad_.atLeftEdge()));
  if (leave) return closeChoice();
  const int i = choicePad_.key(k);
  if (i < 0 || i >= int(choices_.size())) return;
  const Confirm c = confirm_;
  const bool zone = picking_;
  closeChoice();
  if (zone) applyZone(i);
  else applyChoice(c, i);
}

// Each step plays at its level; Set keeps it, Cancel puts the old one back.
void SettingsScreen::volumeKey(const KeyEvent& k) {
  AppState& a = app();
  switch (volume_.key(k)) {
    case Slider::Result::Changed:
      if (volume_.value() > 0) {
        board::setVolume(uint8_t(volume_.value()));
        board::beep(2000, 80);
      }
      break;
    case Slider::Result::Picked:
      a.cfg.buzzer = volume_.value() > 0;
      if (a.cfg.buzzer) a.cfg.volume = uint8_t(volume_.value());
      settings::saveUiPrefs(a.cfg);
      board::setVolume(a.cfg.volume);
      break;
    case Slider::Result::Closed:
      board::setVolume(a.cfg.volume);
      break;
    case Slider::Result::None:
      break;
  }
}

void SettingsScreen::closeVolume() {
  if (!volume_.isOpen()) return;
  volume_.close();
  board::setVolume(app().cfg.volume);
}

void SettingsScreen::onKey(const KeyEvent& k) {
  if (picker_.isOpen()) return picker_.onKey(k);
  if (volume_.isOpen()) return volumeKey(k);
  if (buttons_ && (picking_ || confirm_ != Confirm::None)) return choiceKey(k);
  if (picking_) return zoneKey(k);
  if (confirm_ != Confirm::None) return confirmKey(k);
  // Left leaves from the first column only when the arrows move a cursor.
  const bool leave = k.action == Action::Back ||
                     (k.action == Action::Left && (!board::hasKeys() || pad_.atLeftEdge()));
  if (buttons_ && !leave) {
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
      openChoice(Confirm::Theme, U"theme", std::move(rows), sel);
      break;
    }
    case kTextSize: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (int i = kFirstSize; i < 3; ++i) {
        ListItem it{textSizeLabel(kSizeOrder[i]), U""};
        if (kSizeOrder[i] == app().cfg.textSize) {
          it.marker = true;
          sel = i - kFirstSize;
        }
        rows.push_back(std::move(it));
      }
      openChoice(Confirm::TextSize, U"text size", std::move(rows), sel);
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
      openChoice(Confirm::Rotation, U"orientation", std::move(rows), sel);
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
      openChoice(Confirm::Timeout, U"screen timeout", std::move(rows), sel);
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
      openChoice(Confirm::KbLight, U"keyboard light", std::move(rows), sel);
      break;
    }
    case kNotifyLight: {
      std::vector<ListItem> rows;
      int sel = 0;
      for (size_t i = 0; i < std::size(kLedColorOrder); ++i) {
        ListItem it{ledColorLabel(kLedColorOrder[i]), U""};
        if (kLedColorOrder[i] == app().cfg.notifyLight) {
          it.marker = true;
          sel = int(i);
        }
        rows.push_back(std::move(it));
      }
      openChoice(Confirm::NotifyLight, U"notify light", std::move(rows), sel);
      break;
    }
    case kNotifyTimeout: {
      std::vector<ListItem> rows;
      int sel = 0;
      const uint16_t cur = snapNotifyTimeout(app().cfg.notifyTimeoutSecs);
      for (int i = 0; i < kNotifyTimeoutCount; ++i) {
        ListItem it{notifyTimeoutLabel(kNotifyTimeoutSecs[i]), U""};
        if (kNotifyTimeoutSecs[i] == cur) {
          it.marker = true;
          sel = i;
        }
        rows.push_back(std::move(it));
      }
      openChoice(Confirm::NotifyTimeout, U"light timeout", std::move(rows), sel);
      break;
    }
    case kAtTimeout: {
      AppState& a = app();
      a.cfg.screenSaver = !a.cfg.screenSaver;
      settings::saveScreen(a.cfg.screenOnSecs, a.cfg.screenSaver);
      break;
    }
    case kSound: {
      AppState& a = app();
      if (board::hasVolume()) {
        volume_.open(U"alert sound", a.cfg.buzzer ? a.cfg.volume : 0, kVolumeMax, kVolumeStep, U"Off");
        break;
      }
      a.cfg.buzzer = !a.cfg.buzzer;
      settings::saveUiPrefs(a.cfg);
      if (a.cfg.buzzer) board::beep(2000, 80);
      break;
    }
    case kTapVibrate:
    case kAlertVibrate: {
      AppState& a = app();
      bool& on = item == kTapVibrate ? a.cfg.tapVibrate : a.cfg.alertVibrate;
      on = !on;
      settings::saveVibrate(a.cfg.tapVibrate, a.cfg.alertVibrate);
      break;
    }
    case kSpellCheck:
      app().cfg.spellCheck = !app().cfg.spellCheck;
      settings::saveSpellCheck(app().cfg.spellCheck);
      break;
    case kIdle:
    case kWake: {
      const bool idle = item == kIdle;
      std::vector<ListItem> rows;
      int sel = 0;
      const size_t n = idle ? std::size(kIdleChoices) : std::size(kWakeChoices);
      for (size_t i = 0; i < n; ++i) {
        const uint32_t v = idle ? kIdleChoices[i] : kWakeChoices[i];
        ListItem row{idle ? idleLabel(v) : wakeLabel(v), U""};
        if (v == (idle ? app().cfg.idleSecs : app().cfg.wakeMins)) {
          row.marker = true;
          sel = int(i);
        }
        rows.push_back(std::move(row));
      }
      openChoice(idle ? Confirm::Idle : Confirm::Wake, idle ? U"sleep after" : U"wake every", std::move(rows), sel);
      break;
    }
    case kWebConfig:
      app().cfg.webConfig = !app().cfg.webConfig;
      settings::saveWebConfig(app().cfg.webConfig);
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
#if RC_FEAT_OTA
      const ota::Status st = ota::status();
      if (!st.layoutOk) {
        ui::banner("Updates need a USB install of a release first", true);
        break;
      }
      if (st.state == ota::State::Checking || st.state == ota::State::Installing ||
          st.state == ota::State::Done)
        break;
      if (st.state == ota::State::Available) {
        openConfirm(Confirm::Update, widen(("Install " + st.latest + "? The device restarts.").c_str()), U"Install");
        break;
      }
      if (!ota::startCheck()) ui::banner("Can't check now: is Wi-Fi up?", true);
#endif
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
      openConfirm(Confirm::Logout, U"Log out of Cyberspace?", U"Log out");
      break;
    case kReset:
      openConfirm(Confirm::Reset, U"Erase Wi-Fi, account and settings?", U"Erase everything");
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
  // Every touch board gets buttons here, as Home gets tiles; with keys, a cursor.
  buttons_ = board::hasTouch();
  pad_.setCursor(board::hasKeys());

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
      case kSound:
        rows.push_back(ListItem{U"Alert sound", !a.cfg.buzzer        ? std::u32string(U"off")
                                                : board::hasVolume() ? volumeLabel(a.cfg.volume)
                                                                     : std::u32string(U"on")});
        break;
      case kTapVibrate:
        rows.push_back(ListItem{U"Tap vibration", a.cfg.tapVibrate ? U"on" : U"off"});
        break;
      case kAlertVibrate:
        rows.push_back(ListItem{U"Alert vibration", a.cfg.alertVibrate ? U"on" : U"off"});
        break;
      case kNotifyLight:
        rows.push_back(ListItem{U"Notify light", ledColorLabel(a.cfg.notifyLight)});
        break;
      case kNotifyTimeout:
        rows.push_back(ListItem{U"Light timeout", notifyTimeoutLabel(a.cfg.notifyTimeoutSecs)});
        break;
      case kAtTimeout:
        rows.push_back(ListItem{U"At timeout", a.cfg.screenSaver ? U"screensaver" : U"screen off"});
        break;
      case kSpellCheck:
        rows.push_back(ListItem{U"Spell check", a.cfg.spellCheck ? U"on" : U"off"});
        break;
      case kIdle:
        rows.push_back(ListItem{U"Sleep after", idleLabel(a.cfg.idleSecs)});
        break;
      case kWake:
        rows.push_back(ListItem{U"Wake every", wakeLabel(a.cfg.wakeMins)});
        break;
      case kWebConfig:
        rows.push_back(ListItem{U"Web config", a.cfg.webConfig ? U"on" : U"off", ItemTone::Normal,
                                a.cfg.webConfig});
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
#if RC_FEAT_OTA
        const ota::Status st = ota::status();
        rows.push_back(ListItem{U"Firmware", firmwareValue(st),
                                st.state == ota::State::Failed ? ItemTone::Error : ItemTone::Normal,
                                st.state == ota::State::Available});
#else
        rows.push_back(ListItem{U"Firmware", widen("v" RC_VERSION)});
#endif
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
  // The status lines from row 1, drawn when draw is set and only above row
  // `end`: above the menu on key boards, and on touch-only boards as a header
  // that scrolls with the buttons, `shift` rows already off the top. Returns
  // the rows they take.
  // On a narrow grid a value too long for its row continues on the next rows.
  const bool narrow = narrowGrid(g);
  const int valueW = g.cols() - 11 - (buttons_ ? 1 : 0);  // clear of the scroll bar
  auto status = [&](bool draw, int shift, int end) {
    int r = 1;
    auto field = [&](const char* label, const std::string& value, uint16_t fg) {
      const std::vector<std::u32string> lines =
          narrow ? wrapText(utf8ToU32(value), valueW, valueW)
                 : std::vector<std::u32string>{utf8ToU32(value)};
      for (size_t i = 0; i < lines.size(); ++i, ++r) {
        const int row = r - shift;
        if (!draw || row < 1 || row >= end) continue;
        if (i == 0) putLine(g, 1, row, label, t.dim, t.bg);
        g.putText(11, row, lines[i], fg, t.bg, valueW);
      }
    };
    field("account", a.user.empty() ? "not signed in" : "@" + a.user, t.fg);
    field("wi-fi", wifiUp() ? wifiSsid() + "  " + wifiIp() : "not connected", t.fg);
    field("config", "http://monster-cyberspace.local", t.fg);
    int bat = batteryShown();
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
    return r - 1;
  };
  const bool choosing = picking_ || confirm_ != Confirm::None;
  if (buttons_ && choosing) {
    // The choices as buttons over the whole screen, under a title bar: one
    // column when two would cut the labels.
    const int top = 1;
    for (int c = 0; c < g.cols(); ++c) g.put(c, top, U' ', t.barFg, t.barBg);
    g.putText(1, top, choiceTitle_, t.barFg, t.barBg, g.cols() - 2);
    int labelW = 0;
    for (const ListItem& it : choices_) labelW = std::max(labelW, int(it.label.size()));
    const int cols = ButtonPad::columns();
    const int fit = (g.cols() - 1) / cols - 3;  // inside one button of `cols`
    choicePad_.setCursor(board::hasKeys());
    choicePad_.render(g, t, choices_, top + 1, bottom, false, labelW <= fit ? cols : 1);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF set  back: cancel", U"tap to select  back: cancel");
    return;
  }
  if (buttons_) {
    if (!picking_) {
      const int header = status(false, 0, 0) + 1;  // and a blank row
      pad_.render(g, t, rows, 1, bottom, true, 0, header);
      status(true, pad_.headerShift(), bottom);
    }
  } else {
    status(true, 0, menuTop);
    for (ListItem& it : rows) {
      if (!it.right.empty()) it.label += U": " + it.right;
      it.right.clear();
    }
    menu_.setItems(std::move(rows));
  }

  if (volume_.isOpen()) {
    if (!buttons_) menu_.render(g, menuTop, bottom, 1, g.cols() - 2, t);
    volume_.render(g, 1, bottom, t);
    drawHint(g, t, U"\u2190\u2192 volume  \u25CF set  back: cancel", U"tap the bar  Set or Cancel");
  } else if (picking_) {
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
                      confirm_ == Confirm::NotifyLight || confirm_ == Confirm::NotifyTimeout ||
                      confirm_ == Confirm::Theme || confirm_ == Confirm::Idle ||
                      confirm_ == Confirm::Wake;
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
