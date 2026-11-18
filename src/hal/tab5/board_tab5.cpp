// SPDX-License-Identifier: GPL-3.0-or-later
// M5Stack Tab5 and the Tab5 Keyboard: display, touch, keys, lights and sound.
// Spec: docs/superpowers/specs/2026-11-17-cyberspace-tab5-design.md.
#if defined(BOARD_TAB5)

#include <Arduino.h>
#include <Wire.h>
#include <esp32-hal-hosted.h>
#include <esp_system.h>
#include <hal/wdt_hal.h>

#include <algorithm>
#include <cstring>

#include "hal/board.h"
#include "hal/tab5/audio_es8388.h"
#include "hal/tab5/io_tab5.h"
#include "hal/tab5/keyboard_tab5.h"
#include "hal/tab5/panel_tab5.h"
#include "hal/tab5/pins_tab5.h"
#include "hal/tab5/touch_tab5.h"
#include "input/keymap_tab5.h"
#include "ui/scale2x.h"

namespace board {
namespace {

using namespace tab5;
namespace s2x = rc::scale2x;

// Landscape; with the keyboard fitted the board stays at the landscape
// rotation that sits upright above it (2, checked on hardware 2026-11-17).
constexpr uint8_t kDefaultRotation = 0;
constexpr uint8_t kKeyboardRotation = 2;
uint8_t rotation = kDefaultRotation;  // the UI sets the saved one

// The keyboard, found once at boot (it is fitted with the power off).
bool kbFitted = false;
rc::Tab5Keys keys;

// esp_restart() on the P4 is a CPU reset that leaves the MIPI-DSI host half
// set up, and the panel then stays dark. A shutdown handler turns every
// software restart into a full system reset through the RTC watchdog (as on
// the T-Display P4 and the Tanmatsu). Installed early, so it runs last.
constexpr uint32_t kFullRestartMagic = 0x54423552u;  // "TB5R"
RTC_NOINIT_ATTR uint32_t fullRestartMarker;

void fullRestart() {
  fullRestartMarker = kFullRestartMagic;
  wdt_hal_context_t rwdt = RWDT_HAL_CONTEXT_DEFAULT();
  wdt_hal_write_protect_disable(&rwdt);
  wdt_hal_config_stage(&rwdt, WDT_STAGE0, 20000, WDT_STAGE_ACTION_RESET_SYSTEM);
  wdt_hal_enable(&rwdt);
  wdt_hal_write_protect_enable(&rwdt);
  while (true) {
  }
}

void installFullRestart() {
  if (fullRestartMarker == kFullRestartMagic && esp_reset_reason() == ESP_RST_WDT)
    Serial.println("[tab5] restarted (full reset)");
  fullRestartMarker = 0;
  esp_register_shutdown_handler(fullRestart);
}

// The C6 off and on, so it starts fresh with the host; ESP-Hosted gets the
// Tab5's SDIO pins before WiFi starts (as M5Unified does).
void wifiCoprocessor() {
  io::set(EXP2_ADDR, EXP2_C6_POWER, false);
  delay(100);
  io::set(EXP2_ADDR, EXP2_C6_POWER, true);
  delay(100);
  if (!hostedSetPins(SDIO_CLK, SDIO_CMD, SDIO_D0, SDIO_D1, SDIO_D2, SDIO_D3, SDIO_RST))
    Serial.println("[tab5] hosted pins refused");
}

// ---- Keyboard LEDs ------------------------------------------------------------

// LED 0 shows Caps Lock (white) or a pending Aa (dim white); LED 1 is the
// notification light.
constexpr uint8_t kLedLevel = 0x40, kLedDim = 0x10, kWhite = 7;

void showShiftLed() {
  if (keys.capsLock())
    keyboard::led(0, kWhite, kLedLevel);
  else if (keys.shiftPending())
    keyboard::led(0, kWhite, kLedDim);
  else
    keyboard::led(0, 0, 0);
}

bool pollKeyboard(rc::KeyEvent& key) {
  if (!kbFitted) return false;
  const uint32_t now = millis();
  uint8_t event;
  bool got = false;
  while (!got && keyboard::next(event)) {
    key = keys.key(event, now);
    got = key.action != rc::Action::None;
  }
  showShiftLed();
  if (got) return true;
  key = keys.poll(now);
  return key.action != rc::Action::None;
}

// ---- Display ----------------------------------------------------------------

// Native panel rows [pendingY0, pendingY1) written but not yet flushed out of
// the cache: pushRows gathers them, pollKey (once per UI loop) flushes them.
int pendingY0 = PANEL_H, pendingY1 = 0;

void flushPending() {
  if (pendingY0 >= pendingY1) return;
  panel::flush(pendingY0, pendingY1);
  pendingY0 = PANEL_H;
  pendingY1 = 0;
}

void clearPanel() {
  uint16_t* fb = panel::frame();
  if (!fb) return;
  memset(fb, 0, size_t(PANEL_W) * PANEL_H * 2);
  panel::flush(0, PANEL_H);
  pendingY0 = PANEL_H;
  pendingY1 = 0;
}

}  // namespace

void begin() {
  installFullRestart();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  Wire.setTimeOut(20);  // touch is polled every frame: a stuck read must not stall the UI
  io::begin();  // ends with the panel and touch out of reset
  const panel::Kind kind = touch::detect();
  Serial.printf("[tab5] panel %s\n", panel::kindName(kind));
  wifiCoprocessor();
  if (panel::begin(kind))
    panel::backlight(255);
  else
    Serial.println("[tab5] no panel");
  touch::start();
  kbFitted = keyboard::detect();
  if (kbFitted) rotation = kKeyboardRotation;
  audio::begin();
}

const char* name() { return "Tab5"; }
const char* otaSlug() { return "tab5"; }
uint8_t defaultRotation() { return kDefaultRotation; }

int panelWidth() {
  int w, h;
  s2x::frameSize(NATIVE_W, NATIVE_H, rotation, w, h);
  return w;
}

int panelHeight() {
  int w, h;
  s2x::frameSize(NATIVE_W, NATIVE_H, rotation, w, h);
  return h;
}

// Each frame pixel becomes a 2x2 block of the panel's frame buffer, turned
// for the rotation. The map is affine, so each row walks a fixed step.
void pushRows(const uint16_t* frame, int y0, int y1) {
  uint16_t* fb = panel::frame();
  if (!fb || !frame) return;
  y0 = std::max(y0, 0);
  y1 = std::min(y1, panelHeight());
  if (y0 >= y1) return;
  const int w = panelWidth();
  for (int y = y0; y < y1; ++y) {
    const s2x::Pt a = s2x::frameToNative(0, y, NATIVE_W, NATIVE_H, rotation);
    const s2x::Pt b = s2x::frameToNative(1, y, NATIVE_W, NATIVE_H, rotation);
    const ptrdiff_t step = 2 * (ptrdiff_t(b.y - a.y) * PANEL_W + (b.x - a.x));
    uint16_t* d = fb + 2 * (ptrdiff_t(a.y) * PANEL_W + a.x);
    const uint16_t* s = frame + size_t(y) * w;
    for (int x = 0; x < w; ++x, d += step) {
      const uint16_t c = s[x];
      d[0] = c;
      d[1] = c;
      d[PANEL_W] = c;
      d[PANEL_W + 1] = c;
    }
  }
  int ny0, ny1;
  s2x::nativeRows(y0, y1, NATIVE_W, NATIVE_H, rotation, ny0, ny1);
  pendingY0 = std::min(pendingY0, 2 * ny0);
  pendingY1 = std::max(pendingY1, 2 * ny1);
}

void setRotation(uint8_t r) {
  if (kbFitted) return;
  r &= 3;
  if (r == rotation) return;
  rotation = r;
  clearPanel();
}

bool canRotate() { return !kbFitted; }

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { panel::backlight(level); }
// The backlight only, as M5's own Tab5 apps: the ST7121/ST7123 are TDDI, and
// in SLPIN their touch stops scanning, so a tap could no longer wake it.
void displaySleep(bool sleep) {
  flushPending();
  panel::backlight(sleep ? 0 : 255);
}

bool pollKey(rc::KeyEvent& key) {
  flushPending();
  return pollKeyboard(key);
}

const char* wakeHint() {
  return kbFitted ? "double-tap or press a key to wake" : "double-tap to wake";
}

bool hasTouch() { return true; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!touch::read(rx, ry)) return false;
  const s2x::Pt n = s2x::rawToNative(rx, ry, TOUCH_RAW_W, TOUCH_RAW_H, NATIVE_W, NATIVE_H);
  const s2x::Pt f = s2x::nativeToFrame(n.x, n.y, NATIVE_W, NATIVE_H, rotation);
  x = int16_t(std::clamp(f.x, 0, panelWidth() - 1));
  y = int16_t(std::clamp(f.y, 0, panelHeight() - 1));
  return true;
}

bool hasKeyboard() { return kbFitted; }
bool hasKeys() { return kbFitted; }
bool backspaceIsBack() { return kbFitted; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return true; }
// Tab opens the commands (the keymap's Palette).
const char32_t* menuKeyName() { return kbFitted ? U"tab" : U"ctrl"; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool eink() { return false; }

bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return kbFitted; }
void notifyLight(uint8_t rgb) { keyboard::led(1, rgb, kLedLevel); }

const rc::FunctionKey* functionKeys(size_t& count) {
  if (!kbFitted) {
    count = std::size(rc::kFunctionKeys);
    return rc::kFunctionKeys;
  }
  count = rc::kTab5FunctionKeyCount;
  return rc::kTab5FunctionKeys;
}

void beep(int freqHz, int ms) { audio::beep(freqHz, ms); }
bool hasVolume() { return true; }
void setVolume(uint8_t pct) { audio::setVolume(pct); }

}  // namespace board

#endif
