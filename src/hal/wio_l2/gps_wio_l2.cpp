#if defined(BOARD_WIO_L2)

#include <Arduino.h>

#include "hal/board.h"
#include "hal/wio_l2/io_pca9555.h"
#include "hal/wio_l2/pins_wio_l2.h"

namespace board {
namespace {

bool gpsOn = false;

}  // namespace

bool hasGps() { return true; }

void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    wio::ioSet(wio::EXP_GNSS_POWER, true);
    wio::ioSet(wio::EXP_GNSS_RST, true);
    delay(10);
    wio::ioSet(wio::EXP_GNSS_RST, false);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(wio::GPS_BAUD, SERIAL_8N1, wio::GPS_RX, wio::GPS_TX);
  } else {
    Serial1.end();
    wio::ioSet(wio::EXP_GNSS_POWER, false);
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < static_cast<size_t>(avail) ? n : static_cast<size_t>(avail));
}

// The Wio Tracker L2 has no RTC chip.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

}  // namespace board

#endif  // BOARD_WIO_L2
