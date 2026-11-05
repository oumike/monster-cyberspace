// SPDX-License-Identifier: GPL-3.0-or-later
// Nicolai Electronics Tanmatsu: battery and RTC through the coprocessor (as
// badge-bsp's power.c and tanmatsu_coprocessor read them) and microSD.
#if defined(BOARD_TANMATSU)

#include <Arduino.h>
#include <FS.h>
#include <HWCDC.h>
#include <SD_MMC.h>
#include "hal/board.h"
#include "hal/tanmatsu/coproc_tanmatsu.h"
#include "hal/tanmatsu/pins_tanmatsu.h"
#include "power/battery.h"

namespace board {
namespace {

using namespace tanmatsu;

// Each value keeps its last good reading.
constexpr uint32_t kBatteryMs = 5000;
bool batteryRead = false;
uint32_t batteryMs = 0;
int percent = -1;
float volts = 0.0f;
bool usb = false;

bool readVbat(uint16_t& mv) {
  uint8_t b[2];
  if (!coproc::read(coproc::VBAT, b, 2)) return false;
  mv = uint16_t(b[0] | b[1] << 8);
  return true;
}

// Two reads that agree (rc::tanmatsuVbatAgree), up to three tries.
bool readVbatAgreed(uint16_t& mv) {
  for (int i = 0; i < 3; ++i) {
    uint16_t a, b;
    if (readVbat(a) && readVbat(b) && rc::tanmatsuVbatAgree(a, b, mv)) return true;
  }
  return false;
}

void batterySample() {
  const uint32_t now = millis();
  if (batteryRead && now - batteryMs < kBatteryMs) return;
  batteryRead = true;
  batteryMs = now;
  uint8_t status;
  if (!coproc::read(coproc::CHARGING, &status, 1)) return;
  usb = status & 0x02;
  // No cell fitted: no reading, rather than whatever VBAT floats at.
  if (!(status & 0x01)) {
    volts = 0.0f;
    percent = -1;
    return;
  }
  uint16_t mv;
  if (readVbatAgreed(mv)) {
    volts = mv / 1000.0f;
    percent = rc::tanmatsuBatteryPercent(volts);
  }
}

bool sdMounted = false;

}  // namespace

bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }

bool rtcRead(int64_t& epochUtc) {
  uint8_t b[4];
  if (!coproc::read(coproc::RTC, b, 4)) return false;
  const uint32_t s = uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
  if (s < 1700000000u) return false;  // never set (counts from 0 after a flat battery)
  epochUtc = s;
  return true;
}

bool rtcWrite(int64_t epochUtc) {
  if (epochUtc < 0 || epochUtc > 0xFFFFFFFFll) return false;
  const uint32_t s = uint32_t(epochUtc);
  const uint8_t b[4] = {uint8_t(s), uint8_t(s >> 8), uint8_t(s >> 16), uint8_t(s >> 24)};
  return coproc::write(coproc::RTC, b, 4);
}

int batteryPercent() {
  batterySample();
  return percent;
}

float batteryVolts() {
  batterySample();
  return volts;
}

bool externalPower() {
  batterySample();
  return usb || HWCDC::isPlugged();
}

bool hasSdSlot() { return true; }

// 4-bit on the slot-0 pins, as wadamesh mounts it.
fs::FS* sdMount() {
  if (sdMounted) return &SD_MMC;
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  if (!SD_MMC.begin("/sdcard", false) || SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    return nullptr;
  }
  sdMounted = true;
  return &SD_MMC;
}

void sdUnmount() {
  if (!sdMounted) return;
  SD_MMC.end();
  sdMounted = false;
}

}  // namespace board

#endif
