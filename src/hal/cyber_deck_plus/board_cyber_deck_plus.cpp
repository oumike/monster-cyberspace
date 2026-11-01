#if defined(BOARD_CYBER_DECK_PLUS)

#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>

#include "core_version.h"
#include "hal/board.h"
#include "hal/cyber_deck_plus/io_cyber_deck_plus.h"
#include "hal/cyber_deck_plus/pins_cyber_deck_plus.h"
#include "hal/m9/panel_st7789.h"
#include "input/keymap_cyber_deck.h"
#include "input/keymap_mesh_deck.h"
#include "input/touch_transform.h"

namespace board {
namespace {

constexpr uint32_t kProbeMs = 1500;  // modules can be plugged in later

rc::MeshDeckKeys keys;  // the keyboard module's two 5x5 matrices
rc::MeshDeckPad pad;    // the Core's own front buttons
rc::CyberDeckGamepad gamepad;
rc::KeyEvent pending[8];
int pendingCount = 0;
uint32_t probeMs = 0;

void push(const rc::KeyEvent& e) {
  if (e.action != rc::Action::None && pendingCount < 8) pending[pendingCount++] = e;
}

// One scan feeds three sources. The Core's buttons and the gamepad's D-pad
// both resolve to Up/Down/Left/Right, so either steers; Back arrives from the
// Core's L1 and from the gamepad's L1 and B.
void scan() {
  const uint32_t now = millis();
  if (now - probeMs >= kProbeMs) {
    probeMs = now;
    cyber_deck_plus::keyboardProbe();
    cyber_deck_plus::gamepadProbe();
  }

  cyber_deck_plus::KeyChange ch[8];
  const int n = cyber_deck_plus::keyboardScan(ch, 8);
  for (int i = 0; i < n; ++i) push(keys.key(ch[i].pos, ch[i].pressed, now));
  push(keys.poll(now));

  uint8_t pressed;
  if (cyber_deck_plus::readButtons(pressed)) {
    rc::KeyEvent ev[4];
    const int m = pad.update(pressed, now, ev, 4);
    for (int i = 0; i < m; ++i) push(ev[i]);
  }

  uint8_t left, right;
  if (cyber_deck_plus::readGamepad(left, right)) {
    rc::KeyEvent ev[4];
    const int m = gamepad.update(left, right, now, ev, 4);
    for (int i = 0; i < m; ++i) push(ev[i]);
  }
}

}  // namespace

void begin() {
  Serial.printf("[INFO] [APP] Monster for Cyberspace v%s on Cyber Deck Plus 1.0\n", RC_VERSION);
  Serial.printf("[INFO] [APP] Build: %s %s / toolchain: PlatformIO\n", __DATE__, __TIME__);
  st7789::begin({cyber_deck_plus::TFT_SCK, cyber_deck_plus::TFT_MOSI, -1, cyber_deck_plus::TFT_CS,
                 cyber_deck_plus::TFT_DC, cyber_deck_plus::TFT_RST, cyber_deck_plus::TFT_SPI_HZ,
                 cyber_deck_plus::PANEL_MADCTL, true});
  ledcAttach(cyber_deck_plus::TFT_BL, cyber_deck_plus::TFT_BL_FREQ, 8);
  backlight(false);
  cyber_deck_plus::ioBegin();
  // Media I/O's amplifier starts before any UI: from here on it always has a
  // valid clock and zero samples, never floating inputs.
  Serial.printf("[cyberdeck] NS4168 %s\n",
                cyber_deck_plus::audioBegin() ? "streaming silence" : "not started");
  cyber_deck_plus::keyboardProbe();
  cyber_deck_plus::gamepadProbe();
  probeMs = millis();
  if (!cyber_deck_plus::keyboardPresent()) Serial.println("[cyberdeck] no keyboard module");
  if (!cyber_deck_plus::gamepadPresent()) Serial.println("[cyberdeck] no gamepad module");
  fs::FS* card = sdMount();
  Serial.printf("[cyberdeck] microSD %s\n", card ? "card mounted" : "no card");
  if (card) sdUnmount();
}

const char* name() { return "CyberDeckPlus"; }
const char* otaSlug() { return "cyber-deck-plus"; }
int panelWidth() { return cyber_deck_plus::PANEL_W; }
int panelHeight() { return cyber_deck_plus::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, cyber_deck_plus::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(cyber_deck_plus::TFT_BL, level); }

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    st7789::sleep(true);
  } else {
    st7789::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent& key) {
  if (!pendingCount) scan();
  if (!pendingCount) return false;
  key = pending[0];
  for (int i = 1; i < pendingCount; ++i) pending[i - 1] = pending[i];
  --pendingCount;
  return true;
}

int batteryPercent() { return cyber_deck_plus::batteryPercent(); }

// No charger status line: the gauge's charge rate rising, or a cell held above
// its resting maximum, means USB power.
bool externalPower() {
  int16_t rate;
  if (cyber_deck_plus::batteryRate(rate) && rate > 20) return true;
  return cyber_deck_plus::batteryMv() > 4150;
}

// Media I/O's NS4168, through the always-running I2S stream.
void beep(int freqHz, int ms) { cyber_deck_plus::audioTone(freqHz, ms); }

const char* wakeHint() { return "press R1 to wake"; }

bool hasTouch() { return true; }
bool hasKeyboard() { return cyber_deck_plus::keyboardPresent(); }
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return false; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return true; }
void notifyLight(uint8_t rgb) { cyber_deck_plus::setLed(rgb); }
bool eink() { return false; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!cyber_deck_plus::readTouch(rx, ry)) return false;
  rc::touchToFrame(rx, ry, cyber_deck_plus::PANEL_NATIVE_W, cyber_deck_plus::PANEL_NATIVE_H,
                   cyber_deck_plus::TOUCH_ROT, x, y);
  return true;
}

bool canRotate() { return false; }
void setRotation(uint8_t) {}

// No GPS and no LoRa module in this build: IO17/IO18/IO21 belong to the
// amplifier here, which is the whole reason this target exists.
bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }

// No battery-backed RTC on the Core.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

// Media I/O's microSD, over SDMMC in 1-bit mode rather than SPI: the S3's only
// general-purpose SPI bus is already the panel's.
bool hasSdSlot() { return true; }

fs::FS* sdMount() {
  SD_MMC.end();
  if (!SD_MMC.setPins(cyber_deck_plus::SD_CLK, cyber_deck_plus::SD_CMD, cyber_deck_plus::SD_DAT0)) {
    return nullptr;
  }
  return SD_MMC.begin("/sdcard", true) ? &SD_MMC : nullptr;
}

void sdUnmount() { SD_MMC.end(); }

}  // namespace board

#endif
