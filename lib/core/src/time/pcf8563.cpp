#include "time/pcf8563.h"

#include "time/civil.h"

namespace rc {

namespace {

uint8_t toBcd(int v) { return static_cast<uint8_t>((v / 10) << 4 | (v % 10)); }

bool fromBcd(uint8_t b, int lo, int hi, int& out) {
  if ((b & 0x0F) > 9 || (b >> 4) > 9) return false;
  out = (b >> 4) * 10 + (b & 0x0F);
  return out >= lo && out <= hi;
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && isLeap(y) ? 29 : kDays[m - 1];
}

int64_t floorDiv(int64_t a, int64_t b) { return a / b - (a % b < 0); }

}  // namespace

void pcf8563Encode(int64_t epochSec, uint8_t regs[7]) {
  const int64_t days = floorDiv(epochSec, 86400);
  const int64_t sec = epochSec - days * 86400;
  int y, m, d;
  civilFromDays(days, y, m, d);
  regs[0] = toBcd(static_cast<int>(sec % 60));
  regs[1] = toBcd(static_cast<int>(sec / 60 % 60));
  regs[2] = toBcd(static_cast<int>(sec / 3600));
  regs[3] = toBcd(d);
  regs[4] = static_cast<uint8_t>(((days + 4) % 7 + 7) % 7);  // 1970-01-01 was a Thursday
  regs[5] = toBcd(m);
  regs[6] = toBcd(((y % 100) + 100) % 100);
}

bool pcf8563Decode(const uint8_t regs[7], int64_t& epochSec) {
  if (regs[0] & 0x80) return false;
  int s, mi, h, d, mo, y;
  if (!fromBcd(regs[0] & 0x7F, 0, 59, s) || !fromBcd(regs[1] & 0x7F, 0, 59, mi) ||
      !fromBcd(regs[2] & 0x3F, 0, 23, h) || !fromBcd(regs[3] & 0x3F, 1, 31, d) ||
      !fromBcd(regs[5] & 0x1F, 1, 12, mo) || !fromBcd(regs[6], 0, 99, y)) {
    return false;
  }
  y += 2000;
  if (d > daysInMonth(y, mo)) return false;
  epochSec = (daysFromCivil(y, mo, d) * 24 + h) * 3600 + mi * 60 + s;
  return true;
}

}  // namespace rc
