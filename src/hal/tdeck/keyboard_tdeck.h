#pragma once

#include <cstdint>

// The T-Deck keyboard's ESP32-C3 at I2C 0x55: one byte a read, 0 for none.
namespace tdeck_kb {

bool begin();               // true when 0x55 ACKs (waits up to 500 ms for the C3 to boot)
bool poll(uint8_t& raw);    // at most one read per 10 ms; false on 0 or no data
// The mic key is never sent as a character; only the C3's raw key matrix
// shows it (C0/R6, after camillia-mt). Switching to raw mode can drop a
// character, so the matrix is read only while watching (text entry), at most
// every 90 ms and never while KB_INT says a character is waiting.
void watchMic(bool on);
bool micPressed();          // true once per press while watching
void setBacklight(uint8_t duty);  // the C3's keyboard light, 0..255

}  // namespace tdeck_kb
