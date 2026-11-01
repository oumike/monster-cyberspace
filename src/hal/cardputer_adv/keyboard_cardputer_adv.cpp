#if defined(BOARD_CARDPUTER_ADV)

#include "hal/cardputer_adv/keyboard_cardputer_adv.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/cardputer_adv/pins_cardputer_adv.h"
#include "input/keymap_cardputer_adv.h"

// The Pager's TCA8418 handling (keyboard_pager.cpp), with M5Cardputer 1.1.x's
// matrix: 7 rows by 8 columns (TCA8418.cpp, matrix(7, 8)).

namespace cardputer_kb {
namespace {

constexpr uint8_t kRegCfg = 0x01, kRegIntStat = 0x02, kRegKeyLckEc = 0x03, kRegKeyEventA = 0x04;
constexpr uint8_t kCfgKeyIntEnable = 0x01;
constexpr uint8_t kQueue = 16;
// Read the FIFO this often even with the interrupt line quiet.
constexpr uint32_t kSweepMs = 100;
uint32_t lastDrainMs = 0;

SemaphoreHandle_t busLock = nullptr;
bool present = false;
rc::CardputerKeys keys;
rc::KeyEvent pending[kQueue];
uint8_t head = 0, count = 0;

struct Guard {
  Guard() { i2cLock(); }
  ~Guard() { i2cUnlock(); }
};

bool write(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(cardputer::KB_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

uint8_t read(uint8_t reg) {
  Wire.beginTransmission(cardputer::KB_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom(cardputer::KB_ADDR, uint8_t(1)) != 1) return 0;
  return Wire.read();
}

void push(const rc::KeyEvent& e) {
  if (e.action == rc::Action::None || count == kQueue) return;
  pending[(head + count++) % kQueue] = e;
}

void drain() {
  const uint32_t now = millis();
  Guard g;
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

void i2cLock() {
  if (busLock) xSemaphoreTakeRecursive(busLock, portMAX_DELAY);
}

void i2cUnlock() {
  if (busLock) xSemaphoreGiveRecursive(busLock);
}

bool begin() {
  if (!busLock) busLock = xSemaphoreCreateRecursiveMutex();
  Wire.begin(cardputer::I2C_SDA, cardputer::I2C_SCL, cardputer::I2C_HZ);
  pinMode(cardputer::KB_INT, INPUT_PULLUP);
  Guard g;
  const uint32_t start = millis();
  do {
    Wire.beginTransmission(cardputer::KB_ADDR);
    if (Wire.endTransmission() == 0) present = true;
    else delay(20);
  } while (!present && millis() - start < 500);
  if (present) {
    // Rows R0-R6 and columns C0-C7 in the matrix; C8, C9 plain GPIO inputs.
    for (uint8_t r : {0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B}) write(r, 0x00);
    for (uint8_t r : {0x20, 0x21, 0x22, 0x1A, 0x1B, 0x1C}) write(r, 0xFF);
    write(0x1D, 0x7F);
    write(0x1E, 0xFF);
    write(0x1F, 0x00);
    for (int i = 0; i < 64 && read(kRegKeyEventA); ++i) {
    }
    write(kRegIntStat, 0x03);
    write(kRegCfg, kCfgKeyIntEnable);
  }
  Serial.printf("[kb] keyboard %s\n", present ? "found" : "not found");
  return present;
}

bool poll(rc::KeyEvent& key) {
  if (!present) return false;
  const uint32_t now = millis();
  if (!count && (digitalRead(cardputer::KB_INT) == LOW || now - lastDrainMs >= kSweepMs)) {
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

}  // namespace cardputer_kb

#endif
