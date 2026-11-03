#if defined(BOARD_CYBER_DECK_PLUS)

#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>

#include "core_version.h"
#include "hal/attaky/attaky_core.h"
#include "hal/attaky/pins_attaky_core.h"
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
// Three sources feed one scan: up to kKeyChanges keyboard changes plus its
// repeat, and up to four each from the Core's buttons and the gamepad.
constexpr int kKeyChanges = 8, kPadEvents = 4;
constexpr int kPending = kKeyChanges + 1 + 2 * kPadEvents;

rc::MeshDeckKeys keys;  // the keyboard module's two 5x5 matrices
rc::MeshDeckPad pad;    // the Core's own front buttons
rc::CyberDeckGamepad gamepad;
rc::KeyEvent pending[kPending];
int pendingCount = 0;
uint32_t probeMs = 0;

void push(const rc::KeyEvent& e) {
  if (e.action != rc::Action::None && pendingCount < kPending) pending[pendingCount++] = e;
}

// One scan feeds three sources. The Core's buttons and the gamepad's D-pad
// both resolve to Up/Down/Left/Right, so either steers; Back arrives from the
// Core's L1 and from the gamepad's L1 and B. pollKey scans only once the
// queue is empty, so a full scan always fits.
void scan() {
  const uint32_t now = millis();
  if (now - probeMs >= kProbeMs) {
    probeMs = now;
    attaky_core::keyboardProbe();
    cyber_deck_plus::gamepadProbe();
  }

  attaky_core::KeyChange ch[kKeyChanges];
  const int n = attaky_core::keyboardScan(ch, kKeyChanges);
  for (int i = 0; i < n; ++i) push(keys.key(ch[i].pos, ch[i].pressed, now));
  push(keys.poll(now));

  uint8_t pressed;
  if (attaky_core::readButtons(pressed)) {
    rc::KeyEvent ev[kPadEvents];
    const int m = pad.update(pressed, now, ev, kPadEvents);
    for (int i = 0; i < m; ++i) push(ev[i]);
  }

  uint8_t left, right;
  if (cyber_deck_plus::readGamepad(left, right)) {
    rc::KeyEvent ev[kPadEvents];
    const int m = gamepad.update(left, right, now, ev, kPadEvents);
    for (int i = 0; i < m; ++i) push(ev[i]);
  }
}

}  // namespace

void begin() {
  Serial.printf("[INFO] [APP] Monster for Cyberspace v%s on Cyber Deck Plus 1.0\n", RC_VERSION);
  Serial.printf("[INFO] [APP] Build: %s %s / toolchain: PlatformIO\n", __DATE__, __TIME__);
  st7789::begin({attaky_core::TFT_SCK, attaky_core::TFT_MOSI, -1, attaky_core::TFT_CS,
                 attaky_core::TFT_DC, attaky_core::TFT_RST, attaky_core::TFT_SPI_HZ,
                 attaky_core::PANEL_MADCTL, true});
  ledcAttach(attaky_core::TFT_BL, attaky_core::TFT_BL_FREQ, 8);
  backlight(false);
  cyber_deck_plus::parkMic();
  attaky_core::ioBegin("cyberdeck");
  // Media I/O's amplifier starts before any UI: from here on its lines are
  // driven by I2S, never floating.
  Serial.printf("[cyberdeck] NS4168 %s\n", cyber_deck_plus::audioBegin() ? "ready" : "not started");
  attaky_core::keyboardProbe();
  cyber_deck_plus::gamepadProbe();
  probeMs = millis();
  if (!attaky_core::keyboardPresent()) Serial.println("[cyberdeck] no keyboard module");
  if (!cyber_deck_plus::gamepadPresent()) Serial.println("[cyberdeck] no gamepad module");
  fs::FS* card = sdMount();
  Serial.printf("[cyberdeck] microSD %s\n", card ? "card mounted" : "no card");
  if (card) sdUnmount();
}

const char* name() { return "CyberDeckPlus"; }
const char* otaSlug() { return "cyber-deck-plus"; }
int panelWidth() { return attaky_core::PANEL_W; }
int panelHeight() { return attaky_core::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, attaky_core::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(attaky_core::TFT_BL, level); }

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

int batteryPercent() { return attaky_core::batteryPercent(); }
bool externalPower() { return attaky_core::externalPower(); }

// Media I/O's NS4168, through the I2S task.
void beep(int freqHz, int ms) { cyber_deck_plus::audioTone(freqHz, ms); }
bool hasVolume() { return true; }
void setVolume(uint8_t pct) { cyber_deck_plus::audioVolume(pct); }

// The Core's R2 (rc::kPadR2) toggles the screen, as on the Mesh Deck. The
// gamepad's R1 is left unmapped.
const char* wakeHint() { return "press R2 to wake"; }

bool hasTouch() { return true; }
bool hasKeyboard() { return attaky_core::keyboardPresent(); }
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
void notifyLight(uint8_t rgb) { attaky_core::setLed(rgb); }
bool eink() { return false; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!attaky_core::readTouch(rx, ry)) return false;
  rc::touchToFrame(rx, ry, attaky_core::PANEL_NATIVE_W, attaky_core::PANEL_NATIVE_H,
                   attaky_core::TOUCH_ROT, x, y);
  return true;
}

bool canRotate() { return false; }
void setRotation(uint8_t) {}

// No GPS and no LoRa module in this build: IO17/IO18/IO21 belong to the
// amplifier here.
bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }

// No battery-backed RTC on the Core.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

// Media I/O's microSD, over SDMMC in 1-bit mode: a peripheral of its own, so
// it leaves the panel's SPI bus alone.
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
