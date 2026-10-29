#if defined(BOARD_TDECK)

#include "hal/tdeck/keyboard_tdeck.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdeck/pins_tdeck.h"

namespace tdeck_kb {
namespace {

constexpr uint32_t kPollMs = 10, kProbeMs = 500, kMicPollMs = 90;
constexpr uint8_t kRawCmd = 0x03, kKeyCmd = 0x04, kMatrixBytes = 5;
bool present = false, rawMode = false, watching = false, micHeld = false;
uint32_t lastPollMs = 0, lastMicMs = 0;

void command(uint8_t cmd) {
  Wire.beginTransmission(tdeck::KB_ADDR);
  Wire.write(cmd);
  (void)Wire.endTransmission();
}

// One raw sample; puts the C3 back in key mode, where it reports characters.
bool readMatrix(uint8_t m[kMatrixBytes]) {
  Wire.beginTransmission(tdeck::KB_ADDR);
  Wire.write(kRawCmd);
  if (Wire.endTransmission() != 0) return false;
  const uint8_t count = Wire.requestFrom(tdeck::KB_ADDR, kMatrixBytes);
  uint8_t n = 0;
  while (Wire.available()) {
    uint8_t b = (uint8_t)Wire.read();
    if (n < kMatrixBytes) m[n++] = b;
  }
  command(kKeyCmd);
  return count == kMatrixBytes && n == kMatrixBytes;
}

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
  if (present) {
    pinMode(tdeck::KB_INT, INPUT_PULLUP);
    uint8_t m[kMatrixBytes];
    rawMode = readMatrix(m);
    Serial.printf("[kb] raw matrix %s\n", rawMode ? "available" : "unavailable");
  }
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

void watchMic(bool on) {
  if (!on) micHeld = false;
  watching = on;
}

bool micPressed() {
  if (!present || !rawMode || !watching) return false;
  uint32_t now = millis();
  if (now - lastMicMs < kMicPollMs) return false;
  if (digitalRead(tdeck::KB_INT) == LOW) return false;  // a character is waiting
  lastMicMs = now;
  uint8_t m[kMatrixBytes] = {};
  if (!readMatrix(m)) return false;
  const bool held = (m[0] & (1u << 6)) != 0;
  const bool edge = held && !micHeld;
  micHeld = held;
  return edge;
}

}  // namespace tdeck_kb

#endif
