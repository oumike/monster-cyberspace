#pragma once

#include <cstddef>
#include <cstdint>

#include "input/key_event.h"

// The hardware abstraction each board implements. One implementation is
// compiled per firmware env (src/hal/<board>/).
namespace board {

void begin();
const char* name();  // "M9", "WioL2"; also used in the setup AP name
int panelWidth();
int panelHeight();
// Sends rows [y0, y1) of a full panelWidth() x panelHeight() RGB565 frame.
void pushRows(const uint16_t* frame, int y0, int y1);
void backlight(bool on);
void displaySleep(bool sleep);
// The next pending key press, already mapped to a portable key.
bool pollKey(rc::KeyEvent& key);
int batteryPercent();  // -1 when unknown
bool externalPower();
// Returns at once; the tone plays in the background.
void beep(int freqHz, int ms);
// Raw touch, for boards with a touch panel. readTouch is true while a finger
// is down, with x, y in the panelWidth() x panelHeight() frame.
bool hasTouch();
bool readTouch(int16_t& x, int16_t& y);
// GPS receiver. On: power, a 10 ms reset pulse, then the UART up. Off: the
// UART down, then power off.
bool hasGps();
void gpsPower(bool on);
// Raw NMEA bytes. Never blocks; 0 while off.
size_t gpsRead(uint8_t* buf, size_t n);
// Battery-backed RTC, in UTC seconds. rtcRead is false with no RTC, an I2C
// error, or a time the RTC flags as unreliable (lost power).
bool rtcRead(int64_t& epochUtc);
bool rtcWrite(int64_t epochUtc);

}  // namespace board
