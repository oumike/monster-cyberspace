#if defined(BOARD_TDECK)

#include <Arduino.h>

#include "hal/board.h"
#include "hal/tdeck/pins_tdeck.h"

namespace board {
namespace {

bool gpsOn = false;

}  // namespace

bool hasGps() { return true; }

// The L76K sits on the POWERON rail with everything else, so it stays powered
// (about 20 mA) and "off" only stops the UART.
void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    Serial1.setRxBufferSize(4096);
    Serial1.begin(tdeck::GPS_BAUD, SERIAL_8N1, tdeck::GPS_RX, tdeck::GPS_TX);
  } else {
    Serial1.end();
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < static_cast<size_t>(avail) ? n : static_cast<size_t>(avail));
}

// The T-Deck has no RTC chip.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

}  // namespace board

#endif
