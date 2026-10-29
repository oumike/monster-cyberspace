#if defined(BOARD_TDECK_PRO)

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/tdeck_pro/pins_tdeck_pro.h"

namespace board {
namespace {
bool gpsOn = false;
}  // namespace

bool hasGps() { return true; }

// GPS_EN switches the receiver's supply, so off is really off.
void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    digitalWrite(tdeck_pro::GPS_EN, HIGH);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(tdeck_pro::GPS_BAUD, SERIAL_8N1, tdeck_pro::GPS_RX, tdeck_pro::GPS_TX);
  } else {
    Serial1.end();
    digitalWrite(tdeck_pro::GPS_EN, LOW);
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < static_cast<size_t>(avail) ? n : static_cast<size_t>(avail));
}

// No RTC on this board: the clock comes from NTP or the GPS.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

bool hasSdSlot() { return true; }

// On the shared SPI bus with the e-paper, which begin() brought up. Slower
// clocks are tried for cards that won't start at the first.
fs::FS* sdMount() {
  for (uint32_t hz : {4000000u, 1000000u}) {
    if (SD.begin(tdeck_pro::SD_CS, SPI, hz) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
