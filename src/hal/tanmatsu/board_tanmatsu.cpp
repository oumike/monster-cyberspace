// SPDX-License-Identifier: GPL-3.0-or-later
// Nicolai Electronics Tanmatsu: display, keyboard, buttons, lights and sound.
// Spec: docs/superpowers/specs/2026-11-04-cyberspace-tanmatsu-design.md.
#if defined(BOARD_TANMATSU)

#include <Arduino.h>
#include <esp32-hal-hosted.h>
#include <esp_system.h>
#include <hal/wdt_hal.h>

#include <algorithm>
#include <cstring>

#include "hal/board.h"
#include "hal/tanmatsu/audio_es8156.h"
#include "hal/tanmatsu/coproc_tanmatsu.h"
#include "hal/tanmatsu/panel_st7701.h"
#include "hal/tanmatsu/pins_tanmatsu.h"
#include "input/keymap_tanmatsu.h"
#include "ui/kb_light.h"
#include "ui/scale2x.h"

namespace board {
namespace {

using namespace tanmatsu;
namespace s2x = rc::scale2x;

constexpr uint8_t kRotation = PANEL_ROTATION;

// esp_restart() on the P4 is a CPU reset that leaves the MIPI-DSI host half
// set up, and the panel then stays dark. A shutdown handler turns every
// software restart into a full system reset through the RTC watchdog (as on
// the T-Display P4). Installed early, so it runs last.
constexpr uint32_t kFullRestartMagic = 0x544D5253u;  // "TMRS"
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
    Serial.println("[tanmatsu] restarted (full reset)");
  fullRestartMarker = 0;
  esp_register_shutdown_handler(fullRestart);
}

// The C6 off, then on in application mode, so it starts fresh with the host;
// ESP-Hosted gets the SDIO pins before WiFi starts.
void wifiCoprocessor() {
  coproc::write8(coproc::RADIO, 0);
  delay(100);
  coproc::write8(coproc::RADIO, 2);
  delay(100);
  if (!hostedSetPins(SDIO_CLK, SDIO_CMD, SDIO_D0, SDIO_D1, SDIO_D2, SDIO_D3, SDIO_RST))
    Serial.println("[tanmatsu] hosted pins refused");
}

// ---- Lights -----------------------------------------------------------------

// Six GRB LEDs; manual mode, so LED 0 (power) is ours to light. LED 1 is the
// radio (Wi-Fi) light, LED 2 the one the badge marks for messages.
// Each status LED keeps its rgb bits, lit at one shared level.
constexpr int kPowerLed = 0, kWifiLed = 1, kMessageLed = 2;
uint8_t ledLevelValue = 0x20;
uint8_t ledRgb[3] = {2, 0, 0};  // power green until the setting arrives
uint8_t leds[18] = {};

void writeLeds() {
  for (int i = 0; i < 3; ++i) {
    const uint8_t rgb = ledRgb[i], v = ledLevelValue;
    leds[i * 3] = rgb & 2 ? v : 0;      // G
    leds[i * 3 + 1] = rgb & 1 ? v : 0;  // R
    leds[i * 3 + 2] = rgb & 4 ? v : 0;  // B
  }
  coproc::write(coproc::LEDS, leds, sizeof leds);
}

void setLed(int i, uint8_t rgb) {
  if (ledRgb[i] == rgb) return;
  ledRgb[i] = rgb;
  writeLeds();
}

// ---- Keys -------------------------------------------------------------------

rc::TanmatsuKeys keys;
constexpr uint32_t kKeyPollMs = 15;
uint32_t keyPollMs = 0;

bool pollKeyboard(rc::KeyEvent& key) {
  const uint32_t now = millis();
  if (now - keyPollMs >= kKeyPollMs) {
    keyPollMs = now;
    uint8_t state[rc::kTanmatsuKeyBytes];
    if (coproc::read(coproc::KEYBOARD, state, sizeof state)) keys.update(state, now);
  }
  key = keys.next(now);
  return key.action != rc::Action::None;
}

// Volume down (GPIO35, low pressed) is Down, repeating like the arrows.
constexpr uint32_t kDebounceMs = 30;
bool volDown = false, volRaw = false;
uint32_t volChangedMs = 0, volRepeatMs = 0;

bool pollVolumeDown(rc::KeyEvent& key) {
  const uint32_t now = millis();
  const bool raw = digitalRead(VOLUME_DOWN) == LOW;
  if (raw != volRaw) {
    volRaw = raw;
    volChangedMs = now;
  }
  if (raw != volDown && now - volChangedMs >= kDebounceMs) {
    volDown = raw;
    if (!raw) return false;
    volRepeatMs = now + rc::kTanmatsuRepeatDelayMs;
  } else if (!volDown || int32_t(now - volRepeatMs) < 0) {
    return false;
  } else {
    volRepeatMs = now + rc::kTanmatsuRepeatMs;
  }
  key = rc::KeyEvent{rc::Action::Down};
  return true;
}

// The power button (coprocessor input bit 2): a press sends ScreenOff, which
// locks the screen or, when it is dark, wakes it. Held 2 s, the hardware
// cuts the power itself.
constexpr uint32_t kButtonPollMs = 50;
uint32_t buttonPollMs = 0;
bool powerDown = false;

bool pollPowerButton(rc::KeyEvent& key) {
  const uint32_t now = millis();
  if (now - buttonPollMs < kButtonPollMs) return false;
  buttonPollMs = now;
  uint8_t in;
  if (!coproc::read(coproc::INPUTS, &in, 1)) return false;
  const bool down = in & 0x04;
  const bool pressed = down && !powerDown;
  powerDown = down;
  if (!pressed) return false;
  key = rc::KeyEvent{rc::Action::ScreenOff};
  return true;
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

uint8_t backlightValue = 255;

}  // namespace

void begin() {
  installFullRestart();
  coproc::begin();
  coproc::write8(coproc::DISPLAY_BACKLIGHT, 0);  // dark until the panel is up
  coproc::write8(coproc::LED_MODE, 0);
  writeLeds();
  coproc::write8(coproc::ADC_CONTROL, 0x02);  // continuous: VBAT stays fresh
  wifiCoprocessor();
  if (!panel::begin()) Serial.println("[tanmatsu] no panel");
  coproc::write8(coproc::DISPLAY_BACKLIGHT, backlightValue);
  pinMode(VOLUME_DOWN, INPUT_PULLUP);
  audio::begin();
}

const char* name() { return "Tanmatsu"; }
const char* otaSlug() { return "tanmatsu"; }

int panelWidth() {
  int w, h;
  s2x::frameSize(NATIVE_W, NATIVE_H, kRotation, w, h);
  return w;
}

int panelHeight() {
  int w, h;
  s2x::frameSize(NATIVE_W, NATIVE_H, kRotation, w, h);
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
    const s2x::Pt a = s2x::frameToNative(0, y, NATIVE_W, NATIVE_H, kRotation);
    const s2x::Pt b = s2x::frameToNative(1, y, NATIVE_W, NATIVE_H, kRotation);
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
  s2x::nativeRows(y0, y1, NATIVE_W, NATIVE_H, kRotation, ny0, ny1);
  pendingY0 = std::min(pendingY0, 2 * ny0);
  pendingY1 = std::max(pendingY1, 2 * ny1);
}

void setRotation(uint8_t) {}
bool canRotate() { return false; }

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) {
  backlightValue = level;
  const bool ok = coproc::write8(coproc::DISPLAY_BACKLIGHT, level);
  uint8_t back = 0;
  const bool read = coproc::read(coproc::DISPLAY_BACKLIGHT, &back, 1);
  Serial.printf("[tanmatsu] backlight %u: write %s, reads %d\n", level, ok ? "ok" : "FAILED",
                read ? int(back) : -1);
}
void displaySleep(bool sleep) {
  flushPending();
  panel::sleep(sleep);
}

bool pollKey(rc::KeyEvent& key) {
  flushPending();
  return pollPowerButton(key) || pollVolumeDown(key) || pollKeyboard(key);
}

const char* wakeHint() { return "press the power button to wake"; }

bool hasTouch() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }

bool hasKeyboard() { return true; }
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool eink() { return false; }

bool hasKbLight() { return true; }
uint8_t kbLightDefault() { return uint8_t(rc::KbLight::Medium); }
void kbLight(uint8_t duty, bool screenOn) {
  coproc::write8(coproc::KEYBOARD_BACKLIGHT, screenOn ? duty : 0);
}

bool hasNotifyLight() { return true; }
void notifyLight(uint8_t rgb) { setLed(kMessageLed, rgb); }
bool hasPowerLight() { return true; }
void powerLight(uint8_t rgb) { setLed(kPowerLed, rgb); }
bool hasWifiLight() { return true; }
void wifiLight(uint8_t rgb) { setLed(kWifiLed, rgb); }
bool hasLedLevel() { return true; }
void ledLevel(uint8_t value) {
  if (value == ledLevelValue) return;
  ledLevelValue = value;
  writeLeds();
}

const rc::FunctionKey* functionKeys(size_t& count) {
  count = rc::kTanmatsuFunctionKeyCount;
  return rc::kTanmatsuFunctionKeys;
}

void beep(int freqHz, int ms) { audio::beep(freqHz, ms); }
bool hasVolume() { return true; }
void setVolume(uint8_t pct) { audio::setVolume(pct); }

}  // namespace board

#endif
