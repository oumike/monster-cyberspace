#pragma once

#include <cstdint>

// The Attaky Core's I2C devices, shared by the Mesh Deck and the Cyber Deck
// Plus HALs: the AW9523 front expander (buttons, RGB LED, touch reset), FT6636
// touch, MAX17048 gauge, and the keyboard module's two AW9523 halves. Only
// the UI task calls these, so the bus has no lock.
namespace attaky_core {

// Bus recovery, Wire, the front expander and the touch reset. tag prefixes
// this module's log lines ("meshdeck", "cyberdeck"). False without the front
// expander.
bool ioBegin(const char* tag);

// Raw register access on the shared bus, for a deck's own I2C modules.
bool writeReg(uint8_t addr, uint8_t reg, uint8_t v);
bool readRegs(uint8_t addr, uint8_t reg, uint8_t* out, uint8_t n);

// Front buttons, pressed bits (AW9523 port 0, inverted). False on a bad read.
bool readButtons(uint8_t& pressed);
bool readTouch(int16_t& x, int16_t& y);  // native portrait
int batteryPercent();                    // -1 without a gauge
uint16_t batteryMv();                    // 0 without a gauge
// The gauge's charge rate in 0.208 %/h steps (positive while charging).
bool batteryRate(int16_t& rate);
// No charger status line: the gauge's charge rate rising, or a cell held
// above its resting maximum, means USB power.
bool externalPower();

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

// The LEDs, bit 0 red, 1 green, 2 blue: setLed lights the front one and each
// keyboard half's together; setFrontLed only the front one; setKbLed one
// half's (kKbLeft, kKbRight), kept for when that half is found.
constexpr uint8_t kKbLeft = 0, kKbRight = 1;
void setLed(uint8_t rgb);
void setFrontLed(uint8_t rgb);
void setKbLed(uint8_t half, uint8_t rgb);
bool keyboardHalfPresent(uint8_t half);

}  // namespace attaky_core
