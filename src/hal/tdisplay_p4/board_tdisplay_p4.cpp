// SPDX-License-Identifier: GPL-3.0-or-later
// LilyGO T-Display P4 V1.0 (AMOLED). Bring-up and the full restart from
// wadamesh variants/tdisplay_p4/ (target.cpp, RM69A10Display.cpp).
#if defined(BOARD_TDISPLAY_P4)

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <hal/wdt_hal.h>

#include <algorithm>
#include <cstring>

#include "hal/board.h"
#include "hal/tdisplay_p4/haptic_aw86224.h"
#include "hal/tdisplay_p4/i2c1_tdisplay_p4.h"
#include "hal/tdisplay_p4/io_xl9535.h"
#include "hal/tdisplay_p4/keyboard_tdisplay_p4.h"
#include "hal/tdisplay_p4/panel_rm69a10.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"
#include "hal/tdisplay_p4/touch_gt9895.h"
#include "hal/wio_l2/audio_es8311.h"
#include "input/keymap_tdisplay_p4.h"
#include "ui/kb_light.h"
#include "ui/scale2x.h"

namespace board {
namespace {

using namespace tdp4;
namespace s2x = rc::scale2x;

uint8_t rotation = 1;  // defaultRotation(); the UI sets the saved one

// The keyboard expansion, found once at boot (it is fitted with the power
// off). With it the board stays at rotation 0, landscape above the keys.
bool kbFitted = false;
rc::TdisplayP4Keys keys;
bool ledsShown = false;

// esp_restart() on the P4 is a CPU reset that leaves the MIPI-DSI host half
// set up, and the panel then stays dark. A shutdown handler turns every
// software restart into a full system reset through the RTC watchdog. It is
// installed early, so it runs last (handlers run newest first).
constexpr uint32_t kFullRestartMagic = 0x50345253u;  // "P4RS"
RTC_NOINIT_ATTR uint32_t fullRestartMarker;

void fullRestart() {
  fullRestartMarker = kFullRestartMagic;
  wdt_hal_context_t rwdt = RWDT_HAL_CONTEXT_DEFAULT();
  wdt_hal_write_protect_disable(&rwdt);
  // Slow-clock ticks: about 130 ms at 150 kHz. It only has to be short.
  wdt_hal_config_stage(&rwdt, WDT_STAGE0, 20000, WDT_STAGE_ACTION_RESET_SYSTEM);
  wdt_hal_enable(&rwdt);
  wdt_hal_write_protect_enable(&rwdt);
  while (true) {
  }
}

void installFullRestart() {
  if (fullRestartMarker == kFullRestartMagic && esp_reset_reason() == ESP_RST_WDT)
    Serial.println("[tdp4] restarted (full reset)");
  fullRestartMarker = 0;
  esp_register_shutdown_handler(fullRestart);
}

// The unused SX1262: reset through the XL9535, then SetSleep (0x84, cold
// start), bit-banged in SPI mode 0. False when BUSY never drops.
bool radioSleep() {
  pinMode(LORA_CS, INPUT_PULLUP);  // high before output: a falling edge wakes it
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_SCK, OUTPUT);
  digitalWrite(LORA_SCK, LOW);
  pinMode(LORA_MOSI, OUTPUT);
  digitalWrite(LORA_MOSI, LOW);
  pinMode(LORA_MISO, INPUT_PULLUP);
  pinMode(LORA_BUSY, INPUT_PULLUP);
  xl9535::set(EXP_RADIO_RST, false);
  delay(10);
  xl9535::set(EXP_RADIO_RST, true);
  const uint32_t t0 = millis();
  while (digitalRead(LORA_BUSY) == HIGH) {
    if (millis() - t0 > 50) return false;
    delay(1);
  }
  const uint8_t cmd[] = {0x84, 0x00};
  digitalWrite(LORA_CS, LOW);
  delayMicroseconds(1);
  for (uint8_t b : cmd) {
    for (int i = 7; i >= 0; --i) {
      digitalWrite(LORA_MOSI, (b >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, HIGH);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, LOW);
    }
  }
  delayMicroseconds(1);
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(LORA_MOSI, LOW);
  return true;
}

// The side button: a press sends ScreenOff, which locks the screen or, when
// it is dark, wakes it.
constexpr uint32_t kButtonDebounceMs = 30;
bool buttonDown = false, buttonRaw = false;
uint32_t buttonChangedMs = 0;

bool pollButton(rc::KeyEvent& key) {
  const uint32_t now = millis();
  const bool raw = digitalRead(SIDE_BUTTON) == LOW;
  if (raw != buttonRaw) {
    buttonRaw = raw;
    buttonChangedMs = now;
  }
  if (raw == buttonDown || now - buttonChangedMs < kButtonDebounceMs) return false;
  buttonDown = raw;
  if (!raw) return false;
  key = rc::KeyEvent{rc::Action::ScreenOff};
  return true;
}

// Native panel rows [pendingY0, pendingY1) written but not yet flushed out of
// the cache. In landscape every frame row spans the panel's full height, so
// pushRows only gathers them and pollKey, once per UI loop, flushes them.
int pendingY0 = PANEL_H, pendingY1 = 0;

void flushPending() {
  if (pendingY0 >= pendingY1) return;
  panel::flush(pendingY0, pendingY1);
  pendingY0 = PANEL_H;
  pendingY1 = 0;
}

// The expansion's events through the key map, then a held key's repeats. The
// LEDs follow Caps Lock; F11 asks the app for the next light step.
bool pollKeyboard(rc::KeyEvent& key) {
  if (!kbFitted) return false;
  keyboard::poll();
  uint8_t keyNum;
  bool pressed;
  bool got = false;
  while (!got && keyboard::next(keyNum, pressed)) {
    key = keys.key(keyNum, pressed, millis());
    if (keys.takeLightStep()) key = rc::KeyEvent{rc::Action::KbLightStep};
    got = key.action != rc::Action::None;
  }
  if (keys.capsLock() != ledsShown && keyboard::leds(keys.capsLock())) ledsShown = keys.capsLock();
  if (got) return true;
  key = keys.poll(millis());
  return key.action != rc::Action::None;
}

// The codec shares Wire1 with the keyboard expansion; a bus busy for longer
// than this drops the tone.
bool audioBusTake() { return i2c1::acquire(i2c1::Audio, 50) != nullptr; }
void audioBusGive() { i2c1::release(); }

// Bit 6 is the 5V rail the power-on sequence leaves on, and other parts sit
// on it, so a beep only makes sure it is on and never turns it off. The
// NS4150B amplifier has no enable line of its own.
void audioPower(bool on) {
  if (on) xl9535::set(EXP_AUDIO_POWER, true);
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
  if (xl9535::begin()) xl9535::powerOnSequence();
  if (!panel::begin()) Serial.println("[tdp4] no panel");
  Serial.println(radioSleep() ? "[tdp4] LoRa radio asleep" : "[tdp4] no LoRa radio");
  touch::begin();
  pinMode(SIDE_BUTTON, INPUT_PULLUP);
  kbFitted = keyboard::detect();
  if (kbFitted) rotation = 0;
  es8311::Config audio{CODEC_ADDR, I2S_MCLK, I2S_BCK, I2S_WS, I2S_DOUT, 8, audioPower, nullptr,
                       audioBusGive};
  audio.wire = &Wire1;
  audio.tryLock = audioBusTake;
  es8311::begin(audio);
  tdp4::haptic::begin();
}

const char* name() { return "TDisplayP4"; }
const char* otaSlug() { return "tdisplay-p4"; }
uint8_t defaultRotation() { return 1; }

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
void backlightLevel(uint8_t level) { panel::brightness(level); }
void displaySleep(bool sleep) {
  flushPending();
  panel::sleep(sleep);
}

bool pollKey(rc::KeyEvent& key) {
  flushPending();
  return pollButton(key) || pollKeyboard(key);
}

const char* wakeHint() { return "press the side button to wake"; }

bool hasTouch() { return true; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!touch::read(rx, ry)) return false;
  const s2x::Pt n = s2x::rawToNative(rx, ry, TOUCH_RAW_W, TOUCH_RAW_H, NATIVE_W, NATIVE_H);
  const s2x::Pt f = s2x::nativeToFrame(n.x, n.y, NATIVE_W, NATIVE_H, rotation);
  x = int16_t(f.x);
  y = int16_t(f.y);
  return true;
}

bool hasKeyboard() { return kbFitted; }
bool hasKeys() { return kbFitted; }
bool backspaceIsBack() { return kbFitted; }
bool anyInputWakes() { return kbFitted; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return kbFitted; }
uint8_t kbLightDefault() { return uint8_t(rc::KbLight::Medium); }
void kbLight(uint8_t duty, bool screenOn) { keyboard::light(duty, screenOn); }
bool hasNotifyLight() { return false; }
void notifyLight(uint8_t) {}
bool eink() { return false; }

void beep(int freqHz, int ms) { es8311::beep(freqHz, ms); }
bool hasHaptic() { return true; }
void haptic(int ms) { tdp4::haptic::buzz(ms); }

// GPS, RTC, battery and SD arrive in Task 9.
int batteryPercent() { return -1; }
bool externalPower() { return false; }
bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }
bool hasSdSlot() { return true; }
fs::FS* sdMount() { return nullptr; }
void sdUnmount() {}

}  // namespace board

#endif
