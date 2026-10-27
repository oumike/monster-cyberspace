#if defined(BOARD_PAGER)

#include <Arduino.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/pager/io_pager.h"
#include "hal/pager/pins_pager.h"
#include "time/pcf8563.h"

namespace board {
namespace {

bool gpsOn = false;
// The PCF85063A keeps the PCF8563's time layout (seconds to years, BCD, the
// oscillator-stop flag in bit 7 of seconds) four registers further up.
constexpr uint8_t kRegSeconds = 0x04;

}  // namespace

bool hasGps() { return true; }

// The receiver has a rail of its own (EXP_GPS_EN), so off is really off.
void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    pager::ioSet(pager::EXP_GPS_EN, true);
    pager::ioSet(pager::EXP_GPS_RST, false);
    delay(20);
    pager::ioSet(pager::EXP_GPS_RST, true);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(pager::GPS_BAUD, SERIAL_8N1, pager::GPS_RX, pager::GPS_TX);
  } else {
    Serial1.end();
    pager::ioSet(pager::EXP_GPS_EN, false);
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < static_cast<size_t>(avail) ? n : static_cast<size_t>(avail));
}

bool rtcRead(int64_t& epochUtc) {
  uint8_t regs[7];
  pager::I2cGuard g;
  if (!pager::i2cRead(pager::RTC_ADDR, kRegSeconds, regs, sizeof regs)) return false;
  return rc::pcf8563Decode(regs, epochUtc);
}

bool rtcWrite(int64_t epochUtc) {
  uint8_t regs[7];
  rc::pcf8563Encode(epochUtc, regs);
  pager::I2cGuard g;
  Wire.beginTransmission(pager::RTC_ADDR);
  Wire.write(kRegSeconds);
  Wire.write(regs, sizeof regs);
  return Wire.endTransmission() == 0;
}

}  // namespace board

#endif
