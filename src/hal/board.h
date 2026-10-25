#pragma once

#include <cstdint>

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
// A raw key code from the keyboard controller, if one is pending.
bool pollKey(uint8_t& raw);
// True for early keyboard controllers (hw 0x03, fw 0x10).
bool earlyKeyboard();
int batteryPercent();  // -1 when unknown
bool externalPower();
void beep(int freqHz, int ms);

}  // namespace board
