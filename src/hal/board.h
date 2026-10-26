#pragma once

#include <cstdint>

#include "input/key_event.h"

// The hardware abstraction each board implements. One implementation is
// compiled per firmware env (src/hal/<board>/).
namespace board {

void begin();
const char* name();  // "M9"; also used in the setup AP name
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

}  // namespace board
