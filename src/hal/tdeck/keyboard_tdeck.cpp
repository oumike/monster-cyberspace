#if defined(BOARD_TDECK)

#include "hal/tdeck/keyboard_tdeck.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdeck/pins_tdeck.h"

namespace tdeck_kb {
namespace {

constexpr uint32_t kPollMs = 10, kProbeMs = 500;
bool present = false;
uint32_t lastPollMs = 0;

}  // namespace

bool begin() {
  // The C3 boots after POWERON goes high, so give it a moment to answer.
  uint32_t start = millis();
  do {
    Wire.beginTransmission(tdeck::KB_ADDR);
    if (Wire.endTransmission() == 0) {
      present = true;
      break;
    }
    delay(20);
  } while (millis() - start < kProbeMs);
  Serial.printf("[kb] keyboard %s\n", present ? "found" : "not found");
  return present;
}

bool poll(uint8_t& raw) {
  if (!present) return false;
  uint32_t now = millis();
  if (now - lastPollMs < kPollMs) return false;
  lastPollMs = now;
  if (Wire.requestFrom(tdeck::KB_ADDR, (uint8_t)1) != 1) return false;
  raw = (uint8_t)Wire.read();
  return raw != 0;
}

}  // namespace tdeck_kb

#endif
