#if defined(BOARD_WIO_L2)

#include <Arduino.h>
#include <HWCDC.h>
#include <SD_MMC.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/wio_l2/audio_es8311.h"
#include "hal/wio_l2/io_pca9555.h"
#include "hal/wio_l2/panel_nv3031b.h"
#include "hal/wio_l2/pins_wio_l2.h"
#include "hal/wio_l2/touch_gt911.h"
#include "input/touch_transform.h"
#include "ui/rotation.h"

namespace board {
namespace {

using rc::Action;
using rc::KeyEvent;

uint8_t rotation = 0;

bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

// LP5814 four-channel LED driver behind the panel. Register values from
// camillia-mt's display.h / platy-aprs's display.cpp.
bool lp5814Begin() {
  wio::I2cGuard g;
  bool ok = i2cWrite(wio::LP5814_ADDR, 0x00, 0x01);  // chip enable
  ok = i2cWrite(wio::LP5814_ADDR, 0x01, 0x01) && ok;  // max current 51 mA
  ok = i2cWrite(wio::LP5814_ADDR, 0x02, 0x00) && ok;
  ok = i2cWrite(wio::LP5814_ADDR, 0x04, 0x4E) && ok;  // dimming mode
  ok = i2cWrite(wio::LP5814_ADDR, 0x05, 0xF0) && ok;  // manual mode
  for (uint8_t ch = 0; ch < 4; ++ch) ok = i2cWrite(wio::LP5814_ADDR, 0x14 + ch, 200) && ok;
  ok = i2cWrite(wio::LP5814_ADDR, 0x02, 0x0F) && ok;  // enable all channels
  ok = i2cWrite(wio::LP5814_ADDR, 0x0F, 0x55) && ok;  // latch
  delay(5);
  for (uint8_t ch = 0; ch < 4; ++ch) i2cWrite(wio::LP5814_ADDR, 0x18 + ch, 0);
  if (!ok) Serial.println("[wio] LP5814 backlight not answering");
  return ok;
}

// ADS1115 single shot: AIN0 vs GND, +/-2.048 V, 128 SPS (platy-aprs board.cpp).
bool adsReadMv(uint32_t& mv) {
  Wire.beginTransmission(wio::ADS1115_ADDR);
  Wire.write(0x01);
  Wire.write(0xC5);
  Wire.write(0x83);
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  Wire.beginTransmission(wio::ADS1115_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(wio::ADS1115_ADDR, (uint8_t)2) != 2) return false;
  int16_t raw = (int16_t)((Wire.read() << 8) | Wire.read());
  if (raw <= 0) return false;
  mv = (uint32_t)raw * 2048UL / 32768UL;
  return true;
}

constexpr uint32_t kBatteryMs = 10000;
float battVolts = 0;  // 0: no reading yet
uint32_t battReadMs = 0;

// The sense divider is switched on only for the reading. Cached: a reading
// costs ~30 ms of I2C.
float batteryVolts() {
  wio::I2cGuard g;  // also keeps the UI and web tasks off the cache together
  uint32_t now = millis();
  if (battReadMs && now - battReadMs < kBatteryMs) return battVolts;
  battReadMs = now | 1;
  if (!wio::ioSet(wio::EXP_BATT_SENSE_EN, true)) return battVolts;
  delay(5);
  uint32_t sum = 0;
  int good = 0;
  for (int i = 0; i < 2; ++i) {
    uint32_t mv;
    if (adsReadMv(mv) && mv > 500 && mv < 2400) {
      sum += mv;
      ++good;
    }
  }
  wio::ioSet(wio::EXP_BATT_SENSE_EN, false);
  if (good) battVolts = sum / good * wio::BATT_DIV / 1000.0f;
  return battVolts;
}

// Buttons. The touch panel is the primary input; these two keys stand in for
// gestures that are hard to touch:
//   BOOT tap = Back, BOOT hold = OpenHome,
//   Wake tap = PowerTap (screen off/peek/wake), Wake hold = ScreenOff.
constexpr uint32_t kDebounceMs = 30, kBootHoldMs = 600, kWakeHoldMs = 1000;
constexpr uint32_t kWakeFallbackMs = 200;

struct Button {
  bool down = false, held = false;
  uint32_t since = 0;
};

Button bootBtn, wakeBtn;
uint32_t wakeReadMs = 0;
KeyEvent pending[4];
int pendingCount = 0;

void push(Action a) {
  if (pendingCount < 4) pending[pendingCount++].action = a;
}

void track(Button& b, bool down, uint32_t now, uint32_t holdMs, Action tap, Action hold) {
  if (down && !b.down) {
    b.since = now;
    b.held = false;
  } else if (down && !b.held && now - b.since >= holdMs) {
    b.held = true;
    push(hold);
  } else if (!down && b.down && !b.held && now - b.since >= kDebounceMs) {
    push(tap);
  }
  b.down = down;
}

void scanButtons() {
  uint32_t now = millis();
  track(bootBtn, digitalRead(wio::BOOT_BUTTON) == LOW, now, kBootHoldMs, Action::Back,
        Action::OpenHome);
  // The Wake button is on the expander: read it when the expander flags a
  // change, while a press is being timed, and now and then in case INT is missed.
  if (digitalRead(wio::EXPANDER_INT) == LOW || wakeBtn.down || now - wakeReadMs >= kWakeFallbackMs) {
    wakeReadMs = now;
    track(wakeBtn, wio::ioWakePressed(), now, kWakeHoldMs, Action::PowerTap, Action::ScreenOff);
  }
}

}  // namespace

void begin() {
  // Every rail and reset is on the expander, so it comes first.
  wio::ioBegin();
  lp5814Begin();
  if (!nv3031b::begin()) Serial.println("[wio] panel init failed");
  gt911::begin();
  pinMode(wio::BOOT_BUTTON, INPUT_PULLUP);
  batteryVolts();
  es8311::begin();
}

const char* name() { return "WioL2"; }
int panelWidth() { return rc::rotationPortrait(rotation) ? wio::PANEL_H : wio::PANEL_W; }
int panelHeight() { return rc::rotationPortrait(rotation) ? wio::PANEL_W : wio::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  nv3031b::pushRows(frame, panelWidth(), y0, y1);
}

void backlight(bool on) {
  wio::I2cGuard g;
  for (uint8_t ch = 0; ch < 4; ++ch)
    i2cWrite(wio::LP5814_ADDR, 0x18 + ch, on ? wio::BACKLIGHT_ON : 0);
}

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    nv3031b::sleep(true);
  } else {
    nv3031b::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent& key) {
  if (!pendingCount) {
    scanButtons();
  }
  if (!pendingCount) return false;
  key = pending[0];
  for (int i = 1; i < pendingCount; ++i) pending[i - 1] = pending[i];
  --pendingCount;
  return true;
}

int batteryPercent() {
  float v = batteryVolts();
  if (v < 2.5f) return -1;  // no battery reading
  int pct = (int)((v - 3.3f) / (4.15f - 3.3f) * 100.0f + 0.5f);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

// No charger status on this board. A USB host seen on the S3's own USB port
// means USB power; otherwise, as on the M9, a cell held above its resting
// maximum means it's charging. A plain USB charger is only caught by the latter.
bool externalPower() { return HWCDC::isPlugged() || batteryVolts() > 4.25f; }

void beep(int freqHz, int ms) { es8311::beep(freqHz, ms); }

const char* wakeHint() { return "hold Wake to wake"; }

bool hasTouch() { return gt911::present(); }
bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx = 0, ry = 0;
  const bool down = gt911::read(rx, ry);
  rc::touchToFrame(rx, ry, wio::PANEL_NATIVE_W, wio::PANEL_NATIVE_H, rotation, x, y);
  return down;
}

bool canRotate() { return true; }

void setRotation(uint8_t r) {
  rotation = rc::rotationFromIndex(r);
  nv3031b::setRotation(rotation);
}

bool hasSdSlot() { return true; }

// 1-bit SD_MMC on a rail the expander gates. Power on through the I2C lock,
// mount, and treat a CARD_NONE result as no card. Power stays on only while
// the card is mounted.
fs::FS* sdMount() {
  if (!wio::ioSet(wio::EXP_SD_POWER, true)) return nullptr;
  delay(10);
  if (!SD_MMC.setPins(wio::SDMMC_CLK, wio::SDMMC_CMD, wio::SDMMC_D0)) {
    wio::ioSet(wio::EXP_SD_POWER, false);
    return nullptr;
  }
  if (!SD_MMC.begin("/sdcard", /*mode1bit=*/true, /*format_if_mount_failed=*/false) ||
      SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    wio::ioSet(wio::EXP_SD_POWER, false);
    return nullptr;
  }
  return &SD_MMC;
}

void sdUnmount() {
  SD_MMC.end();
  wio::ioSet(wio::EXP_SD_POWER, false);
}

}  // namespace board

#endif
