#include <algorithm>
#include <functional>
#include <optional>
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
#include "lvui/cell_font.h"
#include "screens/lv_draw.h"
#include "ui/wrap.h"
#include "screens/screens.h"
#include "store/backup_store.h"
#include "store/settings.h"
#include "power/power_policy.h"
#include "tz/zones.h"
#include "ui/brightness.h"
#include "ui/led_level.h"
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
  kBrightness,
  kScreenTimeout,
  kAtTimeout,
  kKbLight,
  kLedLevel,
  kPowerLight,
  kWifiJoined,
  kWifiSetup,
  kWifiLooking,
  kSound,
  kTapVibrate,
  kAlertVibrate,
  kNotifyLight,
  kNotifyMention,
  kNotifyFeed,
  kNotifyCmail,
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
  if (!board::eink()) v.push_back(kBrightness);
  v.push_back(kScreenTimeout);
  v.push_back(kAtTimeout);
  if (board::hasKbLight()) v.push_back(kKbLight);
  if (board::hasLedLevel()) v.push_back(kLedLevel);
  if (board::hasPowerLight()) v.push_back(kPowerLight);
  if (board::hasWifiLight()) {
    v.push_back(kWifiJoined);
    v.push_back(kWifiSetup);
    v.push_back(kWifiLooking);
  }
  if (board::hasSound()) v.push_back(kSound);
  if (board::hasHaptic()) {
    v.push_back(kTapVibrate);
    v.push_back(kAlertVibrate);
  }
  if (board::hasNotifyLight()) {
    v.push_back(kNotifyLight);
    if (RC_FEAT_CIRC) v.push_back(kNotifyMention);
    if (RC_FEAT_ENTRIES) v.push_back(kNotifyFeed);
    if (RC_FEAT_CMAIL) v.push_back(kNotifyCmail);
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

// The text size slider's stops, in size order.
constexpr TextSize kSizeOrder[] = {TextSize::Compact, TextSize::Comfortable, TextSize::Large};
// Low-memory boards (no PSRAM) offer no Compact: its grid does not fit.
constexpr int kFirstSize = RC_LOW_MEM ? 1 : 0;
}  // namespace

void SettingsScreen::enter(const std::string&) {
  confirm_ = Confirm::None;
  closeVolume();
  closeBrightness();
  closeLedLevel();
  sizeSlider_.close();
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
  if (c == Confirm::PowerLight || c == Confirm::WifiJoined || c == Confirm::WifiSetup ||
      c == Confirm::WifiLooking) {
    if (i >= 0 && i < int(std::size(kLedColorOrder))) {
      Settings& s = app().cfg;
      (c == Confirm::PowerLight   ? s.powerLight
       : c == Confirm::WifiJoined ? s.wifiJoined
       : c == Confirm::WifiSetup  ? s.wifiSetup
                                  : s.wifiLooking) = kLedColorOrder[i];
      settings::saveStatusLights(s);
    }
    return;
  }
  if (c == Confirm::NotifyLight || c == Confirm::NotifyMention || c == Confirm::NotifyFeed ||
      c == Confirm::NotifyCmail) {
    if (i >= 0 && i < int(std::size(kLedColorOrder))) {
      Settings& s = app().cfg;
      LedColor& colour = c == Confirm::NotifyLight     ? s.notifyLight
                         : c == Confirm::NotifyMention ? s.notifyMention
                         : c == Confirm::NotifyFeed    ? s.notifyFeed
                                                       : s.notifyCmail;
      colour = kLedColorOrder[i];
      settings::saveNotifyLights(s);
      ui::notifyLightPreview(colour);
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

// Each step shows at its level; Set keeps it, Cancel puts the old one back.
void SettingsScreen::brightnessKey(const KeyEvent& k) {
  switch (brightness_.key(k)) {
    case Slider::Result::Changed:
      ui::previewBrightness(brightness_.value());
      break;
    case Slider::Result::Picked:
      app().cfg.brightness = snapBrightness(brightness_.value());
      settings::saveBrightness(app().cfg.brightness);
      ui::previewBrightness(-1);
      break;
    case Slider::Result::Closed:
      ui::previewBrightness(-1);
      break;
    case Slider::Result::None:
      break;
  }
}

// The LEDs follow each stop; Set keeps it, Cancel puts the old one back.
void SettingsScreen::ledLevelKey(const KeyEvent& k) {
  switch (ledLevel_.key(k)) {
    case Slider::Result::Changed:
      ui::previewLedLevel(ledLevel_.value());
      break;
    case Slider::Result::Picked:
      app().cfg.ledLevel = ledLevelFromIndex(ledLevel_.value(), app().cfg.ledLevel);
      settings::saveStatusLights(app().cfg);
      ui::previewLedLevel(-1);
      break;
    case Slider::Result::Closed:
      ui::previewLedLevel(-1);
      break;
    case Slider::Result::None:
      break;
  }
}

void SettingsScreen::closeLedLevel() {
  if (!ledLevel_.isOpen()) return;
  ledLevel_.close();
  ui::previewLedLevel(-1);
}

void SettingsScreen::openColour(Confirm c, const char32_t* title, LedColor cur) {
  std::vector<ListItem> rows;
  int sel = 0;
  for (size_t i = 0; i < std::size(kLedColorOrder); ++i) {
    ListItem it{ledColorLabel(kLedColorOrder[i]), U""};
    if (kLedColorOrder[i] == cur) {
      it.marker = true;
      sel = int(i);
    }
    rows.push_back(std::move(it));
  }
  openChoice(c, title, std::move(rows), sel);
}

void SettingsScreen::closeBrightness() {
  if (!brightness_.isOpen()) return;
  brightness_.close();
  ui::previewBrightness(-1);
}

// Set applies the size; Cancel leaves it.
void SettingsScreen::sizeKey(const KeyEvent& k) {
  if (sizeSlider_.key(k) != Slider::Result::Picked) return;
  const int s = sizeSlider_.value() + kFirstSize;
  if (s >= kFirstSize && s < 3 && kSizeOrder[s] != app().cfg.textSize) ui::setTextSize(kSizeOrder[s]);
}

void SettingsScreen::onKey(const KeyEvent& k) {
  if (picker_.isOpen()) return picker_.onKey(k);
  if (volume_.isOpen()) return volumeKey(k);
  if (brightness_.isOpen()) return brightnessKey(k);
  if (ledLevel_.isOpen()) return ledLevelKey(k);
  if (sizeSlider_.isOpen()) return sizeKey(k);
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
      std::vector<std::u32string> names;
      int sel = 0;
      for (int i = kFirstSize; i < 3; ++i) {
        if (kSizeOrder[i] == app().cfg.textSize) sel = i - kFirstSize;
        names.push_back(textSizeLabel(kSizeOrder[i]));
      }
      sizeSlider_.openChoices(U"text size", std::move(names), sel);
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
    case kLedLevel: {
      std::vector<std::u32string> names;
      for (int i = 0; i < kLedLevelCount; ++i) names.push_back(ledLevelLabel(LedLevel(i)));
      ledLevel_.openChoices(U"LED brightness", std::move(names), int(app().cfg.ledLevel));
      break;
    }
    case kPowerLight:
      openColour(Confirm::PowerLight, U"power light", app().cfg.powerLight);
      break;
    case kWifiJoined:
      openColour(Confirm::WifiJoined, U"Wi-Fi joined", app().cfg.wifiJoined);
      break;
    case kWifiSetup:
      openColour(Confirm::WifiSetup, U"Wi-Fi setup AP", app().cfg.wifiSetup);
      break;
    case kWifiLooking:
      openColour(Confirm::WifiLooking, U"Wi-Fi looking", app().cfg.wifiLooking);
      break;
    case kBrightness:
      brightness_.open(U"brightness", app().cfg.brightness, kBrightnessMax, kBrightnessStep, U"",
                       kBrightnessMin);
      break;
    case kNotifyLight:
      openColour(Confirm::NotifyLight, U"channel light", app().cfg.notifyLight);
      break;
    case kNotifyMention:
      openColour(Confirm::NotifyMention, U"mention light", app().cfg.notifyMention);
      break;
    case kNotifyFeed:
      openColour(Confirm::NotifyFeed, U"Feed light", app().cfg.notifyFeed);
      break;
    case kNotifyCmail:
      openColour(Confirm::NotifyCmail, U"C-Mail light", app().cfg.notifyCmail);
      break;
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

namespace {

// Each item as a label and its current value; lines join them, buttons put
// the value at the right.
std::vector<ListItem> itemRows(const std::vector<Item>& items, bool buttons, bool ap,
                               const timesvc::Snapshot& ts) {
  const AppState& a = app();
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
      case kLedLevel:
        rows.push_back(ListItem{U"LED brightness", ledLevelLabel(a.cfg.ledLevel)});
        break;
      case kPowerLight:
        rows.push_back(ListItem{U"Power light", ledColorLabel(a.cfg.powerLight)});
        break;
      case kWifiJoined:
        rows.push_back(ListItem{U"Wi-Fi joined", ledColorLabel(a.cfg.wifiJoined)});
        break;
      case kWifiSetup:
        rows.push_back(ListItem{U"Wi-Fi setup AP", ledColorLabel(a.cfg.wifiSetup)});
        break;
      case kWifiLooking:
        rows.push_back(ListItem{U"Wi-Fi looking", ledColorLabel(a.cfg.wifiLooking)});
        break;
      case kBrightness:
        rows.push_back(ListItem{U"Brightness", volumeLabel(a.cfg.brightness)});
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
        rows.push_back(ListItem{U"Channel light", ledColorLabel(a.cfg.notifyLight)});
        break;
      case kNotifyMention:
        rows.push_back(ListItem{U"Mention light", ledColorLabel(a.cfg.notifyMention)});
        break;
      case kNotifyFeed:
        rows.push_back(ListItem{U"Feed light", ledColorLabel(a.cfg.notifyFeed)});
        break;
      case kNotifyCmail:
        rows.push_back(ListItem{U"C-Mail light", ledColorLabel(a.cfg.notifyCmail)});
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
        if (buttons)
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

  return rows;
}

struct StatusField {
  const char* label;
  std::string value;
  uint16_t fg;
};

// The status lines' labels, values and colours.
std::vector<StatusField> statusFields(const Theme& t, bool ap, const timesvc::Snapshot& ts) {
  const AppState& a = app();
  std::vector<StatusField> f;
  auto field = [&](const char* label, const std::string& value, uint16_t fg) {
    f.push_back(StatusField{label, value, fg});
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
  return f;
}

// The status lines from row 1, put(row, label, line, fg) for each line only
// above row `end`, `shift` rows already off the top (label: on a value's
// first line, else null). Returns the rows they take.
// On a narrow grid a value too long for its row continues on the next rows.
template <typename Put>
int layoutStatus(const std::vector<StatusField>& fields, bool narrow, int valueW, int shift, int end, Put put) {
  int r = 1;
  for (const StatusField& f : fields) {
    const std::vector<std::u32string> lines =
        narrow ? wrapText(utf8ToU32(f.value), valueW, valueW)
               : std::vector<std::u32string>{utf8ToU32(f.value)};
    for (size_t i = 0; i < lines.size(); ++i, ++r) {
      const int row = r - shift;
      if (row < 1 || row >= end) continue;
      put(row, i == 0 ? f.label : nullptr, lines[i], f.fg);
    }
  }
  return r - 1;
}

// The menu sits at the bottom and scrolls, so the account, Wi-Fi and
// config lines above it always have room.
int menuTopRow(int bottom, int nItems) {
  constexpr int kStatusRows = 3;
  const int menuRows = std::clamp(bottom - 1 - kStatusRows, std::min(nItems, 4), nItems);
  return bottom - menuRows;
}

}  // namespace

bool SettingsScreen::confirmPicks() const {
  return confirm_ == Confirm::Rotation ||
         confirm_ == Confirm::Timeout || confirm_ == Confirm::KbLight ||
         confirm_ == Confirm::PowerLight || confirm_ == Confirm::WifiJoined ||
         confirm_ == Confirm::WifiSetup || confirm_ == Confirm::WifiLooking ||
         confirm_ == Confirm::NotifyLight || confirm_ == Confirm::NotifyMention ||
         confirm_ == Confirm::NotifyFeed || confirm_ == Confirm::NotifyCmail ||
         confirm_ == Confirm::NotifyTimeout ||
         confirm_ == Confirm::Theme || confirm_ == Confirm::Idle ||
         confirm_ == Confirm::Wake;
}

const Slider* SettingsScreen::openSlider() const {
  if (volume_.isOpen()) return &volume_;
  if (ledLevel_.isOpen()) return &ledLevel_;
  if (brightness_.isOpen()) return &brightness_;
  if (sizeSlider_.isOpen()) return &sizeSlider_;
  return nullptr;
}

void SettingsScreen::build(lv_obj_t* root) {
  v_.make(root);
  // The screen's text size, as ui_task picks it for a compactText() screen.
  TextSize only;
  const std::optional<TextSize> fixed =
      board::fixedTextSize(only) ? std::optional<TextSize>(only) : std::nullopt;
  frame_ = lvui::plainBox(root);
  lv_obj_set_size(frame_, lv_pct(100), lv_pct(100));
  lv_obj_set_style_text_font(frame_, lvui::fontFor(RC_LOW_MEM ? TextSize::Comfortable : TextSize::Compact, fixed),
                             0);
  v_->padView_.create(frame_);
  v_->listView_.create(frame_);
  // Over the buttons, as the grid draws the header lines after them.
  v_->text_.create(frame_);
  zonePanel_ = lvui::plainBox(frame_);
  lvui::addStyle(zonePanel_, lvui::Role::Screen);
  zoneTitle_ = lvui::plainBox(frame_);
  lvui::addStyle(zoneTitle_, lvui::Role::Bar);
  lv_obj_t* zoneLabel = lvui::lineLabel(zoneTitle_);
  lvui::setText(zoneLabel, U"time zone");
  v_->zoneView_.create(frame_);
  choiceRow_ = lvui::plainBox(frame_);
  lvui::addStyle(choiceRow_, lvui::Role::Bar);
  choiceLabel_ = lvui::lineLabel(choiceRow_);
  v_->choiceView_.create(frame_);
  v_->hint_.create(frame_);
  picker_.build(frame_);
  v_->status_.create(frame_);
  // Over the status bar: the grid's overlay backdrop takes the whole screen.
  v_->confirmView_.create(frame_);
  // Last: a slider's backdrop takes every tap, the status bar's too.
  sliderHost_ = lvui::plainBox(frame_);
  lv_obj_set_size(sliderHost_, lv_pct(100), lv_pct(100));
  v_->sliderView_.create(sliderHost_);
  sync();
}

void SettingsScreen::update(int64_t nowMs) {
  const AppState& a = app();
  // The time lines and the Firmware row follow within the gate's second.
  if (gate_.due(v_->text_.obj(), nowMs,
                {wifiUp(), setupApUp(), a.auth, static_cast<int64_t>(std::hash<std::string>{}(a.user)),
                 batteryShown()}))
    sync();
}

void SettingsScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(v_->status_, U"settings", -1);
  picker_.sync();
  const bool pickerOpen = picker_.isOpen();
  buttons_ = board::hasTouch();
  pad_.setCursor(board::hasKeys());
  const bool choosing = picking_ || confirm_ != Confirm::None;
  const Slider* slider = openSlider();
  // Two layouts: the choices as buttons over the whole screen, else the
  // status lines and the menu under whichever of a slider, the zone list or a
  // confirm is open.
  const bool choiceButtons = !pickerOpen && buttons_ && choosing;
  const bool page = !pickerOpen && !choiceButtons;
  const bool zone = page && !slider && picking_;
  const bool confirm = page && !slider && !picking_ && confirm_ != Confirm::None;
  const bool menu = page && !buttons_ && !zone && !confirm;
  lvui::setHidden(choiceRow_, !choiceButtons);
  lvui::setHidden(v_->choiceView_.obj(), !choiceButtons);
  lvui::setHidden(v_->text_.obj(), !page);
  lvui::setHidden(v_->padView_.obj(), !page || !buttons_);
  lvui::setHidden(v_->listView_.obj(), !menu);
  lvui::setHidden(zonePanel_, !zone);
  lvui::setHidden(zoneTitle_, !zone);
  lvui::setHidden(v_->zoneView_.obj(), !zone);
  lvui::setHidden(v_->hint_.obj(), pickerOpen);
  lvui::setHidden(sliderHost_, !page || !slider);
  if (!confirm) lvui::setHidden(v_->confirmView_.backdrop(), true);
  if (pickerOpen) return;

  const lvui::Cell c = v_->text_.cell();
  const int cols = v_->text_.cols(), bottom = v_->text_.rows() - 1;
  if (choiceButtons) {
    // The choices as buttons over the whole screen, under a title bar: one
    // column when two would cut the labels.
    const int top = 1;
    placeCells(choiceRow_, 0, top, cols, top + 1);
    lvui::place(choiceLabel_, c.w, 0, std::max(0, cols - 2) * c.w, c.h);
    lvui::setText(choiceLabel_, choiceTitle_);
    int labelW = 0;
    for (const ListItem& it : choices_) labelW = std::max(labelW, int(it.label.size()));
    const int columns = ButtonPad::columns();
    const int fit = (cols - 1) / columns - 3;  // inside one button of `columns`
    choicePad_.setCursor(board::hasKeys());
    choicePad_.sync(v_->choiceView_, choices_, top + 1, bottom, false, labelW <= fit ? columns : 1);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF set  back: cancel", U"tap to select  back: cancel");
    return;
  }

  const std::vector<Item> items = menuItems();
  const int nItems = int(items.size());
  const bool ap = setupApUp();
  const timesvc::Snapshot ts = timesvc::snapshot();
  std::vector<ListItem> rows = itemRows(items, buttons_, ap, ts);
  const int menuTop = menuTopRow(bottom, nItems);
  const bool narrow = narrowGrid(v_->text_);
  const int valueW = cols - 11 - (buttons_ ? 1 : 0);  // clear of the scroll bar
  const std::vector<StatusField> fields = statusFields(t, ap, ts);
  auto put = [&](int row, const char* label, const std::u32string& line, uint16_t fg) {
    if (label && *label) v_->text_.line(1, row, std::string_view(label), t.dim);
    v_->text_.line(11, row, line, fg, valueW);
  };
  v_->text_.begin();
  if (buttons_) {
    const int header = layoutStatus(fields, narrow, valueW, 0, 0, put) + 1;  // and a blank row
    pad_.sync(v_->padView_, rows, 1, bottom, true, 0, header);
    layoutStatus(fields, narrow, valueW, pad_.headerShift(), bottom, put);
  } else {
    layoutStatus(fields, narrow, valueW, 0, menuTop, put);
    for (ListItem& it : rows) {
      if (!it.right.empty()) it.label += U": " + it.right;
      it.right.clear();
    }
    menu_.setItems(std::move(rows));
  }
  v_->text_.end();
  if (menu) {
    placeCells(v_->listView_.obj(), 1, menuTop, cols - 2, bottom);
    v_->listView_.sync(menu_);
  }

  if (slider) {
    // The box centred over rows [1, bottom), its backdrop over everything.
    lv_obj_t* back = v_->sliderView_.backdrop();
    lv_obj_set_style_pad_top(back, c.h, 0);
    lv_obj_set_style_pad_bottom(back, std::max(0, int(lv_obj_get_height(frame_)) - bottom * c.h), 0);
    v_->sliderView_.sync(*slider);
    if (slider == &volume_)
      syncHint(v_->hint_, U"\u2190\u2192 volume  \u25CF set  back: cancel", U"tap the bar  Set or Cancel");
    else if (slider == &ledLevel_)
      syncHint(v_->hint_, U"\u2190\u2192 LED brightness  \u25CF set  back: cancel", U"tap a stop  Set or Cancel");
    else if (slider == &brightness_)
      syncHint(v_->hint_, U"\u2190\u2192 brightness  \u25CF set  back: cancel", U"tap the bar  Set or Cancel");
    else
      syncHint(v_->hint_, U"\u2190\u2192 size  \u25CF set  back: cancel", U"tap the bar  Set or Cancel");
  } else if (zone) {
    // Over the status lines and the menu, below the status bar.
    const int top = 1;
    placeCells(zoneTitle_, 0, top, cols, top + 1);
    lvui::place(lv_obj_get_child(zoneTitle_, 0), c.w, 0, std::max(0, cols - 2) * c.w, c.h);
    placeCells(zonePanel_, 0, top + 1, cols, bottom);
    placeCells(v_->zoneView_.obj(), 1, top + 1, cols - 2, bottom);
    v_->zoneView_.sync(zoneList_);
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF set  back: cancel", U"tap to select");
  } else if (confirm) {
    // The pickers are centred boxes; the bottom
    // strip only has room for a two-row confirm.
    const bool pick = confirmPicks();
    v_->confirmView_.setPlace(pick ? OverlayPlace::Centre : OverlayPlace::Bottom, bottom * c.h, 0, c.h);
    v_->confirmView_.sync(confirmBox_);
    if (pick)
      syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF set  back: cancel", U"tap: select  outside: cancel");
    else
      syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF confirm  back: cancel", U"tap: confirm  outside: cancel");
  } else if (buttons_) {
    syncHint(v_->hint_, U"", U"tap to select  drag to scroll");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF select  back: home", U"tap to select");
  }
}

}  // namespace rc
