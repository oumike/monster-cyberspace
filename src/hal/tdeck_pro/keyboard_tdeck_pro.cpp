#if defined(BOARD_TDECK_PRO)

#include "hal/tdeck_pro/keyboard_tdeck_pro.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdeck_pro/io_tdeck_pro.h"
#include "hal/tdeck_pro/pins_tdeck_pro.h"
#include "input/keymap_tdeck_pro.h"

// Register setup as on the T-LoRa Pager (the same controller and matrix size),
// from platy-aprs's tdeck_pro_io.cpp.

namespace tdeck_pro_kb {
namespace {

using namespace tdeck_pro;

constexpr uint8_t kRegIntStat = 0x02, kRegKeyLckEc = 0x03, kRegKeyEventA = 0x04;
constexpr uint8_t kQueue = 32;
constexpr uint32_t kSweepMs = 100;
uint32_t lastDrainMs = 0, lastPressMs = 0;

bool present = false;
rc::TdeckProKeys keys;
rc::KeyEvent pending[kQueue];
uint8_t head = 0, count = 0;

bool write(uint8_t reg, uint8_t v) { return i2cWrite(KB_ADDR, reg, v); }

uint8_t read(uint8_t reg) {
  uint8_t v = 0;
  return i2cRead(KB_ADDR, reg, &v, 1) ? v : 0;
}

void push(const rc::KeyEvent& e) {
  if (e.action == rc::Action::None || count == kQueue) return;
  pending[(head + count++) % kQueue] = e;
}

void drain() {
  const uint32_t now = millis();
  I2cGuard g;
  for (int sweep = 0; sweep < 4; ++sweep) {
    const uint8_t n = read(kRegKeyLckEc) & 0x0F;
    write(kRegIntStat, 0x03);
    if (!n) break;
    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t ev = read(kRegKeyEventA);
      if (!ev) break;
      const bool pressed = (ev & 0x80) != 0;
      if (pressed) lastPressMs = now;
      push(keys.key(ev & 0x7F, pressed, now));
    }
  }
}

}  // namespace

bool begin() {
  pinMode(KB_INT, INPUT_PULLUP);
  pinMode(KB_BL, OUTPUT);
  digitalWrite(KB_BL, LOW);  // keys unlit
  I2cGuard g;
  const uint32_t start = millis();
  do {
    Wire.beginTransmission(KB_ADDR);
    if (Wire.endTransmission() == 0) present = true;
    else delay(20);
  } while (!present && millis() - start < 500);
  if (present) {
    for (uint8_t r : {0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B}) write(r, 0x00);
    for (uint8_t r : {0x20, 0x21, 0x22, 0x1A, 0x1B, 0x1C}) write(r, 0xFF);
    write(0x1D, 0x0F);
    write(0x1E, 0xFF);
    write(0x1F, 0x03);
    for (int i = 0; i < 64 && read(kRegKeyEventA); ++i) {
    }
    write(kRegIntStat, 0x03);
  }
  Serial.printf("[kb] keyboard %s\n", present ? "found" : "not found");
  return present;
}

void service() {
  if (!present || digitalRead(KB_INT) != LOW) return;
  lastDrainMs = millis();
  drain();
}

bool poll(rc::KeyEvent& key) {
  if (!present) return false;
  const uint32_t now = millis();
  if (!count && (digitalRead(KB_INT) == LOW || now - lastDrainMs >= kSweepMs)) {
    lastDrainMs = now;
    drain();
  }
  if (!count) push(keys.poll(now));
  if (!count) return false;
  key = pending[head];
  head = (head + 1) % kQueue;
  --count;
  return true;
}

uint32_t lastInputMs() { return lastPressMs; }

}  // namespace tdeck_pro_kb

#endif
