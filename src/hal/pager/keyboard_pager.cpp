#if defined(BOARD_PAGER)

#include "hal/pager/keyboard_pager.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/pager/io_pager.h"
#include "hal/pager/pins_pager.h"
#include "input/keymap_pager.h"

// Register setup and FIFO handling from camillia-mt's keyboard.cpp
// (tloraResetKeyboardController, tloraDrainController).

namespace pager_kb {
namespace {

constexpr uint8_t kRegIntStat = 0x02, kRegKeyLckEc = 0x03, kRegKeyEventA = 0x04;
constexpr uint8_t kQueue = 16;
// Read the FIFO this often even with the interrupt line quiet, in case it
// isn't wired the way camillia-mt's boards have it.
constexpr uint32_t kSweepMs = 100;
uint32_t lastDrainMs = 0;

bool present = false;
rc::PagerKeys keys;
rc::KeyEvent pending[kQueue];
uint8_t head = 0, count = 0;

bool write(uint8_t reg, uint8_t v) { return pager::i2cWrite(pager::KB_ADDR, reg, v); }

uint8_t read(uint8_t reg) {
  uint8_t v = 0;
  return pager::i2cRead(pager::KB_ADDR, reg, &v, 1) ? v : 0;
}

void push(const rc::KeyEvent& e) {
  if (e.action == rc::Action::None || count == kQueue) return;
  pending[(head + count++) % kQueue] = e;
}

void drain() {
  const uint32_t now = millis();
  pager::I2cGuard g;
  // Bounded: a controller stuck reporting events can't spin this forever.
  for (int sweep = 0; sweep < 4; ++sweep) {
    const uint8_t n = read(kRegKeyLckEc) & 0x0F;
    // Acknowledged before popping, so a key struck mid-drain raises it again.
    write(kRegIntStat, 0x03);
    if (!n) break;
    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t ev = read(kRegKeyEventA);  // the FIFO head pops on read
      if (!ev) break;
      push(keys.key(ev & 0x7F, (ev & 0x80) != 0, now));
    }
  }
}

}  // namespace

bool begin() {
  pinMode(pager::KB_INT, INPUT_PULLUP);
  ledcAttach(pager::KB_BL, pager::TFT_BL_FREQ, 8);
  ledcWrite(pager::KB_BL, 0);  // keys unlit
  pager::I2cGuard g;
  uint32_t start = millis();
  do {
    Wire.beginTransmission(pager::KB_ADDR);
    if (Wire.endTransmission() == 0) present = true;
    else delay(20);
  } while (!present && millis() - start < 500);
  if (present) {
    // 4 rows by 10 columns of matrix, the rest plain GPIO inputs.
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

void setBacklight(uint8_t duty) { ledcWrite(pager::KB_BL, duty); }

bool poll(rc::KeyEvent& key) {
  if (!present) return false;
  const uint32_t now = millis();
  if (!count && (digitalRead(pager::KB_INT) == LOW || now - lastDrainMs >= kSweepMs)) {
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

}  // namespace pager_kb

#endif
