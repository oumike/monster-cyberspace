// SPDX-License-Identifier: GPL-3.0-or-later
// LilyGO T-Display P4: GPS, RTC, battery and SD. The gauge reads and the SD
// slot's power, pull-ups and LDO follow wadamesh (variants/tdisplay_p4/
// target.cpp, tdisplay_p4/main/main.cpp and build.sh).
#if defined(BOARD_TDISPLAY_P4)

#include <Arduino.h>
#include <FS.h>
#include <HWCDC.h>
#include <Wire.h>
#include <driver/sdmmc_host.h>
// ldoSdmmc*() are esp32-hal-ldo internals, written against Arduino-ESP32
// core 3.3.12 (framework-arduinoespressif32); recheck them when it changes.
#include <esp32-hal-ldo.h>
#include <esp_vfs_fat.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>
#include <vfs_api.h>

#include "hal/board.h"
#include "hal/tdisplay_p4/io_xl9535.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"
#include "time/pcf8563.h"

namespace board {
namespace {

using namespace tdp4;

// ---- GPS --------------------------------------------------------------------

bool gpsOn = false;

// ---- RTC --------------------------------------------------------------------

constexpr uint8_t kRtcSeconds = 0x02;

// ---- Battery ----------------------------------------------------------------

// BQ27220 standard commands, 16-bit little-endian.
constexpr uint8_t kGaugeVoltage = 0x08, kGaugeAvgCurrent = 0x14, kGaugeSoc = 0x2C;
// The gauge shares Wire with the expander, the RTC and touch, so a lost read
// now and then is normal: each value keeps its last good reading.
constexpr uint32_t kGaugeMs = 5000;
bool gaugeRead = false;
uint32_t gaugeMs = 0;
int percent = -1;
float volts = 0.0f;
bool charging = false;

bool gaugeU16(uint8_t cmd, uint16_t& out) {
  Wire.beginTransmission(GAUGE_ADDR);
  Wire.write(cmd);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(GAUGE_ADDR), 2) != 2) return false;
  const uint16_t lo = Wire.read(), hi = Wire.read();
  out = uint16_t(lo | hi << 8);
  return true;
}

void gaugeSample() {
  const uint32_t now = millis();
  if (gaugeRead && now - gaugeMs < kGaugeMs) return;
  gaugeRead = true;
  gaugeMs = now;
  uint16_t v;
  if (gaugeU16(kGaugeSoc, v) && v <= 100) percent = v;
  if (gaugeU16(kGaugeVoltage, v) && v >= 2500 && v <= 4600) volts = v / 1000.0f;
  // Positive while the pack charges.
  if (gaugeU16(kGaugeAvgCurrent, v)) charging = int16_t(v) > 0;
}

// ---- SD ---------------------------------------------------------------------

// Mounted through ESP-IDF rather than Arduino's SD_MMC: the generic esp32p4
// variant names GPIO45 (keyboard SCL here) the SD power pin, so SD_MMC.begin
// and end would drive it and tear down Wire1, and SD_MMC leaves the slot's
// internal pull-ups off, which wadamesh found the card needs.
constexpr const char* kMountPoint = "/sdcard";
constexpr uint32_t kSdPowerUpMs = 150;

class SdFs : public fs::FS {
 public:
  SdFs() : FS(fs::FSImplPtr(new VFSImpl())) {}
  void mountpoint(const char* path) { _impl->mountpoint(path); }
};

SdFs sdFs;
sdmmc_card_t* card = nullptr;

void releaseLdo(sd_pwr_ctrl_handle_t pwr) {
  sd_pwr_ctrl_del_on_chip_ldo(pwr);
  ldoSdmmcDriverDetached(SD_LDO_CHAN);
}

bool sdTryMount(int khz) {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.flags = SDMMC_HOST_FLAG_4BIT;
  host.max_freq_khz = khz;
  // Slot 0 is IO_MUX only: no pins to route. 0, not GPIO_NUM_NC, marks a pin
  // unset (as Arduino's SD_MMC does for this slot).
  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.clk = slot.cmd = slot.d0 = slot.d1 = slot.d2 = slot.d3 = GPIO_NUM_0;
  slot.d4 = slot.d5 = slot.d6 = slot.d7 = GPIO_NUM_0;
  slot.cd = SDMMC_SLOT_NO_CD;
  slot.wp = SDMMC_SLOT_NO_WP;
  slot.width = 4;
  slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  // The pads' LDO: Arduino's pin manager may hold it for the keyboard's pins
  // on the same rail, and hands it over for the card here.
  sd_pwr_ctrl_ldo_config_t ldoCfg = {};
  ldoCfg.ldo_chan_id = SD_LDO_CHAN;
  sd_pwr_ctrl_handle_t pwr = nullptr;
  ldoSdmmcPrepareAcquire(SD_LDO_CHAN);
  if (sd_pwr_ctrl_new_on_chip_ldo(&ldoCfg, &pwr) != ESP_OK) {
    ldoSdmmcDriverCreateFailed(SD_LDO_CHAN);
    return false;
  }
  ldoSdmmcDriverAttached(SD_LDO_CHAN);
  host.pwr_ctrl_handle = pwr;

  esp_vfs_fat_sdmmc_mount_config_t mountCfg = {};
  mountCfg.format_if_mount_failed = false;
  mountCfg.max_files = 5;
  if (esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot, &mountCfg, &card) != ESP_OK) {
    card = nullptr;
    releaseLdo(pwr);
    return false;
  }
  sdFs.mountpoint(kMountPoint);
  return true;
}

}  // namespace

bool hasGps() { return true; }

// The L76K has no rail of its own: XL9535 bit 9 wakes it, low parks it.
void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    xl9535::set(EXP_GPS_WAKE, true);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  } else {
    Serial1.end();
    xl9535::set(EXP_GPS_WAKE, false);
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
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(kRtcSeconds);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(RTC_ADDR), int(sizeof regs)) != sizeof regs) return false;
  for (uint8_t& r : regs) r = Wire.read();
  return rc::pcf8563Decode(regs, epochUtc);
}

bool rtcWrite(int64_t epochUtc) {
  uint8_t regs[7];
  rc::pcf8563Encode(epochUtc, regs);
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(kRtcSeconds);
  Wire.write(regs, sizeof regs);
  return Wire.endTransmission() == 0;
}

int batteryPercent() {
  gaugeSample();
  return percent;
}

float batteryVolts() {
  gaugeSample();
  return volts;
}

bool externalPower() {
  gaugeSample();
  return charging || HWCDC::isPlugged();
}

bool hasSdSlot() { return true; }

// The slot is powered (XL9535 bit 13 low) only while mounted. A slower clock
// is tried for cards that won't start at the first.
fs::FS* sdMount() {
  if (card) return &sdFs;
  xl9535::set(EXP_SD_POWER, false);
  delay(kSdPowerUpMs);
  for (int khz : {SDMMC_FREQ_DEFAULT, 5000}) {
    if (sdTryMount(khz)) return &sdFs;
  }
  xl9535::set(EXP_SD_POWER, true);
  return nullptr;
}

void sdUnmount() {
  if (card) {
    sd_pwr_ctrl_handle_t pwr = card->host.pwr_ctrl_handle;
    esp_vfs_fat_sdcard_unmount(kMountPoint, card);
    card = nullptr;
    sdFs.mountpoint(nullptr);
    if (pwr) releaseLdo(pwr);
  }
  xl9535::set(EXP_SD_POWER, true);
}

}  // namespace board

#endif
