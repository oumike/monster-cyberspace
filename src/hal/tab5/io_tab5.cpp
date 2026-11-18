// SPDX-License-Identifier: GPL-3.0-or-later
// Register values from M5Unified's Power_Class.inl (Tab5) and
// M5Tab5-UserDemo's m5stack_tab5.c, with the 5 V outputs driven on.
#if defined(BOARD_TAB5)

#include "hal/tab5/io_tab5.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/tab5/pins_tab5.h"

namespace tab5::io {
namespace {

constexpr uint8_t kRegDir = 0x03, kRegOut = 0x05, kRegHiZ = 0x07, kRegPullEn = 0x0B,
                  kRegPullSel = 0x0D, kRegIn = 0x0F;

struct Init {
  uint8_t addr, out, hiZ, pull, dir;
};

// 0x43: antenna internal (P0 low), amplifier off, 5 V out on, LCD_RST an
// input with a pull-up (it must never be driven high), touch and camera out
// of reset. 0x44: C6 powered, USB-A 5 V on, charge current 1 A (P5 low),
// charger on; P4 (power off) low.
constexpr Init kInit[] = {
    {EXP1_ADDR, 0b01110100, 0b00001000, 0b00010000, 0b01100111},
    {EXP2_ADDR, 0b10001001, 0b00000110, 0b00000000, 0b10111001},
};

uint8_t shadow[2] = {kInit[0].out, kInit[1].out};
SemaphoreHandle_t mutex = nullptr;

int slot(uint8_t addr) { return addr == EXP1_ADDR ? 0 : addr == EXP2_ADDR ? 1 : -1; }

bool write(uint8_t addr, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool begin() {
  if (!mutex) mutex = xSemaphoreCreateMutex();
  // M5's Tab5 UserDemo (bsp_reset_tp, which the factory firmware runs): the
  // panel and touch held in reset 100 ms with TP_INT pulled high (its
  // gpio_reset_pin), then 100 ms for the controller to start. Without it the
  // ST7121's touch reports one stuck point.
  pinMode(TOUCH_INT, INPUT_PULLUP);
  constexpr uint8_t kResets = uint8_t(1u << EXP1_LCD_RST | 1u << EXP1_TP_RST);
  Init held[] = {kInit[0], kInit[1]};
  held[0].out &= uint8_t(~kResets);
  held[0].dir |= kResets;
  bool ok = true;
  for (const Init& e : held) {
    // Levels first, then the direction, so no output starts at the wrong one.
    ok = write(e.addr, kRegOut, e.out) && write(e.addr, kRegHiZ, e.hiZ) &&
         write(e.addr, kRegPullSel, e.pull) && write(e.addr, kRegPullEn, e.pull) &&
         write(e.addr, kRegDir, e.dir) && ok;
  }
  delay(100);
  ok = write(EXP1_ADDR, kRegDir, kInit[0].dir) && write(EXP1_ADDR, kRegOut, kInit[0].out) && ok;
  delay(100);
  pinMode(TOUCH_INT, INPUT);
  if (!ok) Serial.println("[tab5] IO expander init failed");
  return ok;
}

void set(uint8_t addr, uint8_t bit, bool high) {
  const int i = slot(addr);
  if (i < 0 || !mutex) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const uint8_t v = high ? shadow[i] | uint8_t(1u << bit) : shadow[i] & uint8_t(~(1u << bit));
  if (v != shadow[i] && write(addr, kRegOut, v)) shadow[i] = v;
  xSemaphoreGive(mutex);
}

bool get(uint8_t addr, uint8_t bit) {
  Wire.beginTransmission(addr);
  Wire.write(kRegIn);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(addr), 1) != 1) return false;
  return (Wire.read() >> bit) & 1;
}

}  // namespace tab5::io

#endif
