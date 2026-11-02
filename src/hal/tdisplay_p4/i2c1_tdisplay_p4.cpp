// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/P4I2c1.cpp.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/i2c1_tdisplay_p4.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::i2c1 {
namespace {

constexpr uint32_t kKeyboardHz = 100000, kAudioHz = 400000;

SemaphoreHandle_t mutex = nullptr;
portMUX_TYPE createMux = portMUX_INITIALIZER_UNLOCKED;
int current = -1;  // the route Wire1 is begun on; -1 for none
bool failLogged[2] = {};

}  // namespace

TwoWire* acquire(Route route, uint32_t waitMs) {
  if (!mutex) {
    SemaphoreHandle_t m = xSemaphoreCreateMutex();
    portENTER_CRITICAL(&createMux);
    if (!mutex) {
      mutex = m;
      m = nullptr;
    }
    portEXIT_CRITICAL(&createMux);
    if (m) vSemaphoreDelete(m);
    if (!mutex) return nullptr;
  }
  const TickType_t ticks = waitMs == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(waitMs);
  if (xSemaphoreTake(mutex, ticks) != pdTRUE) return nullptr;

  // Begun again when it is on the other route, and also when it is not
  // running at all: a bus only remembered as begun fails every transaction.
  if (current != int(route) || !i2cIsInit(1)) {
    if (i2cIsInit(1)) Wire1.end();
    const bool audio = route == Audio;
    const bool ok = audio ? Wire1.begin(AUDIO_SDA, AUDIO_SCL, kAudioHz)
                          : Wire1.begin(KB_SDA, KB_SCL, kKeyboardHz);
    // Not recorded when it failed, so the next call tries again.
    current = ok ? int(route) : -1;
    if (!ok) {
      if (!failLogged[audio]) {
        failLogged[audio] = true;
        Serial.printf("[tdp4] Wire1 for the %s failed to start\n", audio ? "audio" : "keyboard");
      }
      xSemaphoreGive(mutex);
      return nullptr;
    }
  }
  return &Wire1;
}

void release() {
  if (mutex) xSemaphoreGive(mutex);
}

}  // namespace tdp4::i2c1

#endif
