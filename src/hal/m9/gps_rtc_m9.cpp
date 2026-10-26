#if defined(BOARD_M9)

#include <Arduino.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/m9/pins_m9.h"
#include "time/pcf8563.h"

namespace board {
namespace {

bool gpsOn = false;
bool rtcBusUp = false;
constexpr uint8_t kRegSeconds = 0x02;

void rtcBus() {
  if (rtcBusUp) return;
  Wire1.begin(m9::RTC_SDA, m9::RTC_SCL, m9::RTC_I2C_HZ);
  rtcBusUp = true;
}

}  // namespace

bool hasGps() { return true; }

void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    digitalWrite(m9::GPS_EN, LOW);
    digitalWrite(m9::GPS_RST, HIGH);
    delay(10);
    digitalWrite(m9::GPS_RST, LOW);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(m9::GPS_BAUD, SERIAL_8N1, m9::GPS_RX, m9::GPS_TX);
  } else {
    Serial1.end();
    digitalWrite(m9::GPS_EN, HIGH);
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
  rtcBus();
  Wire1.beginTransmission(m9::RTC_ADDR);
  Wire1.write(kRegSeconds);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(m9::RTC_ADDR, static_cast<uint8_t>(7)) != 7) return false;
  uint8_t regs[7];
  for (uint8_t& r : regs) r = Wire1.read();
  return rc::pcf8563Decode(regs, epochUtc);
}

bool rtcWrite(int64_t epochUtc) {
  rtcBus();
  uint8_t regs[7];
  rc::pcf8563Encode(epochUtc, regs);
  Wire1.beginTransmission(m9::RTC_ADDR);
  Wire1.write(kRegSeconds);
  Wire1.write(regs, sizeof regs);
  return Wire1.endTransmission() == 0;
}

}  // namespace board

#endif  // BOARD_M9
