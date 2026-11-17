// SPDX-License-Identifier: GPL-3.0-or-later
// M5Stack Tab5: battery (INA226, as M5Unified's Power_Class), RX8130CE RTC
// and microSD (mounted through ESP-IDF, as the T-Display P4).
#if defined(BOARD_TAB5)

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
#include "hal/tab5/pins_tab5.h"
#include "power/battery.h"
#include "time/pcf8563.h"

namespace board {
namespace {

using namespace tab5;

// ---- Battery ----------------------------------------------------------------

// INA226: 16 averages, 1.1 ms bus and shunt conversions, continuous.
constexpr uint8_t kInaConfig = 0x00, kInaShunt = 0x01, kInaBus = 0x02;
constexpr uint16_t kInaConfigValue = (2u << 9) | (4u << 6) | (4u << 3) | 7u;
// The shunt reads negative while the pack charges (M5Unified inverts it).
// 2.5 uV a step across 5 mOhm: -20 steps is 10 mA in.
constexpr int16_t kChargingShunt = -20;
constexpr uint32_t kBatteryMs = 5000;
bool inaReady = false, batteryRead = false;
uint32_t batteryMs = 0;
int percent = -1;
float volts = 0.0f;
bool charging = false;

bool inaRead(uint8_t reg, uint16_t& v) {
  Wire.beginTransmission(INA226_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(INA226_ADDR), 2) != 2) return false;
  const uint16_t hi = Wire.read(), lo = Wire.read();
  v = uint16_t(hi << 8 | lo);
  return true;
}

bool inaBegin() {
  Wire.beginTransmission(INA226_ADDR);
  Wire.write(kInaConfig);
  Wire.write(uint8_t(kInaConfigValue >> 8));
  Wire.write(uint8_t(kInaConfigValue));
  return Wire.endTransmission() == 0;
}

void batterySample() {
  const uint32_t now = millis();
  if (batteryRead && now - batteryMs < kBatteryMs) return;
  batteryRead = true;
  batteryMs = now;
  if (!inaReady && !(inaReady = inaBegin())) return;
  uint16_t bus, shunt;
  if (inaRead(kInaBus, bus)) {
    const uint32_t mv = uint32_t(bus) * 125 / 100;  // 1.25 mV a step
    percent = rc::tab5BatteryPercent(mv);
    volts = percent < 0 ? 0.0f : mv / 1000.0f;
  }
  if (inaRead(kInaShunt, shunt)) charging = percent >= 0 && int16_t(shunt) < kChargingShunt;
}

// ---- RTC --------------------------------------------------------------------

// RX8130CE: seconds, minutes, hours, weekday (one bit), day, month, year,
// BCD from 0x10; the PCF8563 has day and weekday the other way round.
constexpr uint8_t kRtcSeconds = 0x10;

// ---- SD ---------------------------------------------------------------------

// Mounted through ESP-IDF rather than Arduino's SD_MMC: the generic esp32p4
// variant names GPIO45 the SD power pin and SD_MMC would drive it, and
// SD_MMC leaves the slot's internal pull-ups off.
constexpr const char* kMountPoint = "/sdcard";

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

bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }

bool rtcRead(int64_t& epochUtc) {
  uint8_t r[7];
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(kRtcSeconds);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(RTC_ADDR), int(sizeof r)) != sizeof r) return false;
  for (uint8_t& b : r) b = Wire.read();
  const uint8_t pcf[7] = {uint8_t(r[0] & 0x7F), r[1], r[2], r[4], 0, r[5], r[6]};
  int64_t t;
  if (!rc::pcf8563Decode(pcf, t) || t < 1700000000) return false;  // never set
  epochUtc = t;
  return true;
}

bool rtcWrite(int64_t epochUtc) {
  uint8_t pcf[7];
  rc::pcf8563Encode(epochUtc, pcf);
  const uint8_t r[7] = {pcf[0], pcf[1], pcf[2], uint8_t(1u << pcf[4]), pcf[3], pcf[5], pcf[6]};
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(kRtcSeconds);
  Wire.write(r, sizeof r);
  return Wire.endTransmission() == 0;
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
  return charging || percent < 0 || HWCDC::isPlugged();
}

bool hasSdSlot() { return true; }

// A slower clock is tried for cards that won't start at the first.
fs::FS* sdMount() {
  if (card) return &sdFs;
  for (int khz : {SDMMC_FREQ_DEFAULT, 5000}) {
    if (sdTryMount(khz)) return &sdFs;
  }
  return nullptr;
}

void sdUnmount() {
  if (!card) return;
  sd_pwr_ctrl_handle_t pwr = card->host.pwr_ctrl_handle;
  esp_vfs_fat_sdcard_unmount(kMountPoint, card);
  card = nullptr;
  sdFs.mountpoint(nullptr);
  if (pwr) releaseLdo(pwr);
}

}  // namespace board

#endif
