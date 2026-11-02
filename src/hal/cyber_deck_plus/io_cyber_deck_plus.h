#pragma once

#include <cstdint>

// The Cyber Deck Plus's I2C devices: the Core's AW9523 expanders (front
// buttons, RGB LED), FT6636 touch, MAX17048 gauge, the keyboard module's two
// AW9523 halves and the gamepad's two TCA9555 halves. Only the UI task calls
// these, so the bus has no lock. Media I/O's NS4168 amplifier is on I2S, not
// I2C (see audioBegin).
namespace cyber_deck_plus {

// Bus recovery, Wire, the front expander and the touch reset. False without
// the front expander.
bool ioBegin();
// Front buttons, pressed bits (AW9523 port 0, inverted). False on a bad read.
bool readButtons(uint8_t& pressed);
bool readTouch(int16_t& x, int16_t& y);  // native portrait
int batteryPercent();                    // -1 without a gauge
uint16_t batteryMv();                    // 0 without a gauge
// The gauge's charge rate in 0.208 %/h steps (positive while charging).
bool batteryRate(int16_t& rate);

// Probes any keyboard half not yet found (the module can be plugged in later).
void keyboardProbe();
bool keyboardPresent();
// Scans both halves and reports each debounced change: pos is
// rc::meshDeckKeyPos. Returns the count written.
struct KeyChange {
  uint8_t pos;
  bool pressed;
};
int keyboardScan(KeyChange* out, int max);

// The Core's front LED and each keyboard half's: bit 0 red, 1 green, 2 blue.
void setLed(uint8_t rgb);

// ── Gamepad (Control: Gamepad (Bottom) 1.0) ─────────────────────────────
// Both TCA9555 halves are all-input; pressed bits come back set. Datasheet
// pin map, left half on 0x22 and right on 0x26.
constexpr uint8_t kGpUp = 1 << 5, kGpDown = 1 << 2, kGpLeft = 1 << 4, kGpRight = 1 << 3;
constexpr uint8_t kGpL1 = 1 << 6, kGpL2 = 1 << 7, kGpFnL1 = 1 << 0, kGpFnL2 = 1 << 1;
constexpr uint8_t kGpA = 1 << 3, kGpB = 1 << 4, kGpX = 1 << 2, kGpY = 1 << 5;
constexpr uint8_t kGpR1 = 1 << 1, kGpR2 = 1 << 0, kGpFnR1 = 1 << 7, kGpFnR2 = 1 << 6;

// Brings up any half not yet found. False when neither answers.
bool gamepadProbe();
bool gamepadPresent();
// One read of both halves; false if neither answered this time.
bool readGamepad(uint8_t& left, uint8_t& right);

// ── Media I/O audio (NS4168 over I2S) ───────────────────────────────────
// Starts I2S and a task that free-runs silence. An amplifier with a valid
// clock and zero samples is silent; one with floating inputs is not, so the
// stream never stops once this returns true.
bool audioBegin();
bool audioReady();
// Queues a tone; returns at once and the task plays it.
void audioTone(int freqHz, int ms);

}  // namespace cyber_deck_plus
