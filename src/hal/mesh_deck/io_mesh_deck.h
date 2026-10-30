#pragma once

#include <cstdint>

// The Mesh Deck's I2C devices: the AW9523 expanders (front buttons, keyboard
// halves, RGB LEDs), FT6636 touch and MAX17048 gauge. Only the UI task calls
// these, so the bus has no lock.
namespace mesh_deck {

// Bus recovery, Wire, the front expander and the touch reset. False without
// the front expander.
bool ioBegin();
// Resets the LoRa module's SX1262, if one is fitted, and puts it to sleep
// with its chip select held high (left awake it whines). False: no radio.
bool radioSleep();
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

// The front LED and each keyboard half's: bit 0 red, 1 green, 2 blue.
void setLed(uint8_t rgb);

}  // namespace mesh_deck
