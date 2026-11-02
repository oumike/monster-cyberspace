// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/P4Keyboard.cpp.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/keyboard_tdisplay_p4.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdisplay_p4/i2c1_tdisplay_p4.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::keyboard {
namespace {

// XL9555 registers.
constexpr uint8_t XL_OUT0 = 0x02, XL_CFG0 = 0x06, XL_CFG1 = 0x07;
constexpr uint8_t kLedMask = 0x38;  // port 0 bits 3-5, lit low
// TCA8418 registers.
constexpr uint8_t TCA_INT_STAT = 0x02, TCA_KEY_LCK_EC = 0x03, TCA_KEY_EVENT_A = 0x04,
                  TCA_GPIO_INT_EN_1 = 0x1A, TCA_KP_GPIO_1 = 0x1D, TCA_GPI_EM_1 = 0x20,
                  TCA_GPIO_DIR_1 = 0x23, TCA_GPIO_INT_LVL_1 = 0x26, TCA_DEBOUNCE_DIS_1 = 0x29;

constexpr uint32_t kBacklightHz = 20000;
constexpr uint32_t kBootWaitMs = 100;  // nothing else uses Wire1 yet
constexpr uint32_t kBusWaitMs = 5;
// Read the FIFO this often even with INT quiet.
constexpr uint32_t kSweepMs = 120;

bool present = false;
uint32_t lastDrainMs = 0;

struct Event {
  uint8_t keyNum;
  bool pressed;
};
constexpr uint8_t kQueue = 32;
Event queue[kQueue];
uint8_t head = 0, count = 0;

void push(uint8_t keyNum, bool pressed) {
  if (count == kQueue) return;  // full: keep what was typed, in order
  queue[(head + count++) % kQueue] = Event{keyNum, pressed};
}

bool regWrite(TwoWire* w, uint8_t addr, uint8_t reg, uint8_t val) {
  w->beginTransmission(addr);
  w->write(reg);
  w->write(val);
  return w->endTransmission() == 0;
}

bool regRead(TwoWire* w, uint8_t addr, uint8_t reg, uint8_t& val) {
  w->beginTransmission(addr);
  w->write(reg);
  if (w->endTransmission(false) != 0) return false;
  if (w->requestFrom(addr, uint8_t(1)) != 1) return false;
  val = uint8_t(w->read());
  return true;
}

// Caps Lock lights all three, as LilyGO's keyboard example does. The rest of
// port 0 is read back and left as it was.
bool setLeds(TwoWire* w, bool on) {
  uint8_t out = 0xFF, cfg = 0xFF;
  if (!regRead(w, KB_EXP_ADDR, XL_OUT0, out) || !regRead(w, KB_EXP_ADDR, XL_CFG0, cfg))
    return false;
  out = on ? uint8_t(out & ~kLedMask) : uint8_t(out | kLedMask);
  cfg &= uint8_t(~kLedMask);
  return regWrite(w, KB_EXP_ADDR, XL_OUT0, out) && regWrite(w, KB_EXP_ADDR, XL_CFG0, cfg);
}

// The expansion's XL9555 answers both configuration registers, the TCA8418's
// reset is pulsed through it (low 10 ms, then high), and the TCA8418 answers.
bool findBoth(TwoWire* w) {
  uint8_t out = 0xFF, cfg0 = 0xFF, cfg1 = 0xFF;
  if (!regRead(w, KB_EXP_ADDR, XL_CFG0, cfg0) || !regRead(w, KB_EXP_ADDR, XL_CFG1, cfg1) ||
      !regRead(w, KB_EXP_ADDR, XL_OUT0, out))
    return false;
  const uint8_t bit = uint8_t(1u << KB_EXP_TCA_RST);
  cfg0 &= uint8_t(~bit);
  out &= uint8_t(~bit);
  if (!regWrite(w, KB_EXP_ADDR, XL_OUT0, out) || !regWrite(w, KB_EXP_ADDR, XL_CFG0, cfg0))
    return false;
  delay(10);
  out |= bit;
  if (!regWrite(w, KB_EXP_ADDR, XL_OUT0, out)) return false;
  delay(10);
  w->beginTransmission(KB_ADDR);
  return w->endTransmission() == 0;
}

// Seven rows by ten columns of matrix (wadamesh, after Meshtastic's
// tlora-pager profile), the FIFO emptied and the interrupt cleared.
void setupMatrix(TwoWire* w) {
  for (uint8_t i = 0; i < 3; ++i) {
    regWrite(w, KB_ADDR, TCA_GPIO_DIR_1 + i, 0x00);
    regWrite(w, KB_ADDR, TCA_GPI_EM_1 + i, 0xFF);
    regWrite(w, KB_ADDR, TCA_GPIO_INT_LVL_1 + i, 0x00);
    regWrite(w, KB_ADDR, TCA_GPIO_INT_EN_1 + i, 0xFF);
    regWrite(w, KB_ADDR, TCA_DEBOUNCE_DIS_1 + i, 0x00);
  }
  regWrite(w, KB_ADDR, TCA_KP_GPIO_1, 0x7F);      // rows 0-6
  regWrite(w, KB_ADDR, TCA_KP_GPIO_1 + 1, 0xFF);  // columns 0-7
  regWrite(w, KB_ADDR, TCA_KP_GPIO_1 + 2, 0x03);  // columns 8-9
  uint8_t ev = 0;
  for (int i = 0; i < 16 && regRead(w, KB_ADDR, TCA_KEY_EVENT_A, ev) && ev; ++i) {
  }
  regWrite(w, KB_ADDR, TCA_INT_STAT, 0x03);
}

void drain(TwoWire* w) {
  // Bounded: four sweeps is forty events, far more than one poll finds.
  for (int sweep = 0; sweep < 4; ++sweep) {
    uint8_t n = 0;
    if (!regRead(w, KB_ADDR, TCA_KEY_LCK_EC, n)) return;
    n &= 0x0F;
    // Acknowledged before popping, so a key struck mid-drain raises INT again.
    regWrite(w, KB_ADDR, TCA_INT_STAT, 0x03);
    if (!n) return;
    for (uint8_t i = 0; i < n; ++i) {
      uint8_t ev = 0;
      if (!regRead(w, KB_ADDR, TCA_KEY_EVENT_A, ev) || !ev) return;
      push(ev & 0x7F, (ev & 0x80) != 0);
    }
  }
}

}  // namespace

bool detect() {
  TwoWire* w = i2c1::acquire(i2c1::Keyboard, kBootWaitMs);
  if (!w) return false;
  const bool found = findBoth(w);
  if (found) {
    setupMatrix(w);
    setLeds(w, false);
  }
  i2c1::release();
  Serial.println(found ? "[tdp4] keyboard expansion found" : "[tdp4] no keyboard expansion");
  if (!found) return false;
  // Pulled down here, up on the expansion: low while events wait.
  pinMode(KB_INT, INPUT_PULLDOWN);
  if (!ledcAttach(KB_BL, kBacklightHz, 8)) Serial.println("[tdp4] keyboard light PWM failed");
  ledcWrite(KB_BL, 0);
  present = true;
  return true;
}

void poll() {
  if (!present) return;
  const uint32_t now = millis();
  if (digitalRead(KB_INT) != LOW && now - lastDrainMs < kSweepMs) return;
  TwoWire* w = i2c1::acquire(i2c1::Keyboard, kBusWaitMs);
  if (!w) return;
  lastDrainMs = now;
  drain(w);
  i2c1::release();
}

bool next(uint8_t& keyNum, bool& pressed) {
  if (!count) return false;
  keyNum = queue[head].keyNum;
  pressed = queue[head].pressed;
  head = (head + 1) % kQueue;
  --count;
  return true;
}

void light(uint8_t duty, bool screenOn) {
  if (present) ledcWrite(KB_BL, screenOn ? duty : 0);
}

bool leds(bool on) {
  if (!present) return true;
  TwoWire* w = i2c1::acquire(i2c1::Keyboard, kBusWaitMs);
  if (!w) return false;
  setLeds(w, on);  // a failed write is not retried: it would only fail again
  i2c1::release();
  return true;
}

}  // namespace tdp4::keyboard

#endif
