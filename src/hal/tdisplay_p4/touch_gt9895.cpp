#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/touch_gt9895.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdisplay_p4/io_tdisplay_p4.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace gt9895 {
namespace {

using namespace tdisplay_p4;

constexpr uint32_t kFirmwareAddr = 0x00010014, kRuntimeAddr = 0x00010070;
constexpr size_t kFirmwareSize = 28, kRuntimeMin = 64, kRuntimeMax = 1024;
constexpr size_t kHeader = 8, kPerContact = 8, kChecksum = 2;
constexpr uint8_t kMaxContacts = 10, kTouchEvent = 0x80;

uint32_t reportAddr = 0;
uint8_t runtime[kRuntimeMax];

uint16_t le16(const uint8_t* d) { return uint16_t(d[0] | d[1] << 8); }
uint32_t le32(const uint8_t* d) { return uint32_t(d[0]) | uint32_t(d[1]) << 8 | uint32_t(d[2]) << 16 | uint32_t(d[3]) << 24; }

bool checksumOk(const uint8_t* d, size_t n) {
  if (n < kChecksum) return false;
  uint32_t sum = 0;
  for (size_t i = 0; i < n - kChecksum; ++i) sum += d[i];
  return uint16_t(sum) == le16(d + n - kChecksum);
}

void sendAddr(uint32_t a) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(uint8_t(a >> 24));
  Wire.write(uint8_t(a >> 16));
  Wire.write(uint8_t(a >> 8));
  Wire.write(uint8_t(a));
}

bool readReg(uint32_t a, uint8_t* d, size_t n) {
  sendAddr(a);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, n, true) != n) return false;
  for (size_t i = 0; i < n; ++i) d[i] = uint8_t(Wire.read());
  return true;
}

void clearStatus() {
  sendAddr(reportAddr);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
}

bool probe() {
  uint8_t fw[kFirmwareSize] = {};
  if (!readReg(kFirmwareAddr, fw, sizeof fw) || !checksumOk(fw, sizeof fw) || fw[10] != '9' || fw[11] != '8' ||
      fw[12] != '9' || fw[13] != '5')
    return false;
  uint8_t lenBytes[2];
  if (!readReg(kRuntimeAddr, lenBytes, 2)) return false;
  const size_t len = le16(lenBytes);
  if (len < kRuntimeMin || len > kRuntimeMax) return false;
  if (!readReg(kRuntimeAddr, runtime, len) || !checksumOk(runtime, len)) return false;
  // Skip five length-prefixed arrays to the touch report address.
  size_t off = 2 + 16 + 10 + 4;
  for (int i = 0; i < 5; ++i) {
    if (off >= len - 2) return false;
    const size_t bytes = size_t(runtime[off++]) * 2;
    if (bytes > len - 2 - off) return false;
    off += bytes;
  }
  if (off + 48 > len - 2) return false;
  reportAddr = le32(runtime + off + 44);
  return reportAddr != 0;
}

}  // namespace

bool begin() {
  ioSet(EXP_TOUCH_RST, false);
  delay(30);
  ioSet(EXP_TOUCH_RST, true);
  delay(100);
  I2cGuard g;
  Wire.setBufferSize(kRuntimeMax);
  if (!probe()) {
    reportAddr = 0;
    Serial.println("[gt9895] not found");
    return false;
  }
  return true;
}

bool present() { return reportAddr != 0; }

// No new report (status 0) between scans keeps the last contact, for a while.
bool read(int16_t& x, int16_t& y) {
  static bool down = false;
  static int16_t lx = 0, ly = 0;
  static uint32_t lastMs = 0;
  if (!reportAddr) return false;
  I2cGuard g;
  uint8_t h[kHeader];
  if (!readReg(reportAddr, h, sizeof h) || h[0] == 0) {
    if (down && millis() - lastMs > 100) down = false;
    x = lx, y = ly;
    return down;
  }
  const uint8_t n = h[2] & 0x0F;
  if (!checksumOk(h, sizeof h) || !(h[0] & kTouchEvent) || n == 0 || n > kMaxContacts) {
    clearStatus();
    down = false;
    return false;
  }
  uint8_t p[kMaxContacts * kPerContact + kChecksum];
  const size_t len = n * kPerContact + kChecksum;
  const bool good = readReg(reportAddr + kHeader, p, len) && checksumOk(p, len);
  clearStatus();
  if (good) {
    const int rx = le16(p + 2) * PANEL_W / TOUCH_RAW_W, ry = le16(p + 4) * PANEL_H / TOUCH_RAW_H;
    lx = int16_t(rx < PANEL_W ? rx : PANEL_W - 1);
    ly = int16_t(ry < PANEL_H ? ry : PANEL_H - 1);
    down = true;
    lastMs = millis();
  }
  x = lx, y = ly;
  return down;
}

}  // namespace gt9895

#endif
