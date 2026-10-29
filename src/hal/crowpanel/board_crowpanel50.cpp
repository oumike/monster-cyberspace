#if defined(BOARD_CROWPANEL_50)

#include <Arduino.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/crowpanel/panel_rgb800.h"
#include "hal/crowpanel/pins_crowpanel.h"
#include "hal/wio_l2/touch_gt911.h"
#include "ui/rotation.h"

// Elecrow CrowPanel Advance 5.0": touch only, no keyboard, buttons, SD (its
// pins belong to the wireless slot), GPS or battery sense. A touch wakes it.
// The backlight and the touch reset belong to a PCA9557 on V1.0 boards and a
// board MCU from V1.1; begin() tells them apart by which address answers.

namespace board {
namespace {

using namespace crowpanel::p50;

enum class Ctl : uint8_t { None, Pca9557, Mcu };
Ctl ctl = Ctl::None;
uint8_t pcaOut = 0;
uint8_t rotation = 0;

constexpr uint8_t kPcaRegOutput = 0x01, kPcaRegConfig = 0x03;

uint8_t pinMask(uint8_t pin) { return static_cast<uint8_t>(1u << pin); }

bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void pcaWrite(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(IOEX_ADDR);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

void pcaSet(uint8_t pin, bool high) {
  const uint8_t next = high ? (pcaOut | pinMask(pin)) : (pcaOut & ~pinMask(pin));
  if (next == pcaOut) return;
  pcaOut = next;
  pcaWrite(kPcaRegOutput, pcaOut);
}

bool mcuSend(uint8_t cmd) {
  Wire.beginTransmission(MCU_ADDR);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

void beginController() {
  if (probe(IOEX_ADDR)) {
    ctl = Ctl::Pca9557;
    // Levels first, then direction: backlight on, touch out of reset,
    // amplifier held off.
    pcaOut = pinMask(IOEX_BL) | pinMask(IOEX_TP_RST) | pinMask(IOEX_AMP_SHUT);
    pcaWrite(kPcaRegOutput, pcaOut);
    pcaWrite(kPcaRegConfig,
             static_cast<uint8_t>(~(pinMask(IOEX_BL) | pinMask(IOEX_TP_RST) | pinMask(IOEX_AMP_MUTE) | pinMask(IOEX_AMP_SHUT))));
    Serial.println("[crowpanel] V1.0 board (PCA9557)");
  } else if (probe(MCU_ADDR)) {
    ctl = Ctl::Mcu;
    Serial.println("[crowpanel] V1.1+ board (MCU)");
  } else {
    Serial.println("[crowpanel] no backlight controller found");
  }
}

// INT held LOW through the reset straps the GT911 to 0x5D. Elecrow repeats
// the MCU's wake command until the GT911 answers, so this does too.
void resetTouch() {
  if (ctl == Ctl::None) return;
  for (int attempt = 0; attempt < 5; ++attempt) {
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
    if (ctl == Ctl::Pca9557) {
      pcaSet(IOEX_TP_RST, false);
      delay(20);
      pcaSet(IOEX_TP_RST, true);
      delay(100);
    } else {
      mcuSend(MCU_TOUCH_ON);
      delay(120);
    }
    pinMode(TOUCH_INT, INPUT);
    delay(100);
    if (probe(0x5D) || probe(0x14)) return;
  }
}

}  // namespace

void begin() {
  Wire.begin(crowpanel::I2C_SDA, crowpanel::I2C_SCL);
  beginController();
  backlight(false);
  if (!rgb800::begin()) Serial.println("[crowpanel] panel init failed");
  resetTouch();
  // Only the UI task uses Wire on this board, so no lock.
  gt911::begin({&Wire, crowpanel::TOUCH_ADDR, nullptr, nullptr});
}

const char* name() { return "CrowPanel50"; }
const char* otaSlug() { return "crowpanel50"; }
int panelWidth() { return rc::rotationPortrait(rotation) ? PANEL_H : PANEL_W; }
int panelHeight() { return rc::rotationPortrait(rotation) ? PANEL_W : PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  rgb800::pushRows(frame, panelWidth(), y0, y1, rotation);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }

// The MCU takes MCU_BL_MAX (brightest) to MCU_BL_MIN, or MCU_BL_OFF.
void backlightLevel(uint8_t level) {
  if (ctl == Ctl::Pca9557) {
    pcaSet(IOEX_BL, level > 0);
  } else if (ctl == Ctl::Mcu) {
    const uint32_t span = MCU_BL_MIN - MCU_BL_MAX;
    mcuSend(level == 0 ? MCU_BL_OFF : uint8_t(MCU_BL_MIN - uint32_t(level - 1) * span / 254));
  }
}

// The RGB peripheral keeps scanning the framebuffer; dark is backlight off.
void displaySleep(bool sleep) { backlight(!sleep); }

bool pollKey(rc::KeyEvent&) { return false; }

int batteryPercent() { return -1; }
bool externalPower() { return true; }
void beep(int, int) {}

const char* wakeHint() { return "double-tap to wake"; }

bool hasTouch() { return gt911::present(); }
bool hasKeyboard() { return false; }
bool hasKeys() { return false; }
bool backspaceIsBack() { return false; }
bool anyInputWakes() { return true; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool eink() { return false; }
bool doubleTapWakes() { return true; }

// Raw touch is the panel's native landscape 800x480; turn it into the frame
// as panel_rgb800.cpp turns the frame into the framebuffer.
bool readTouch(int16_t& x, int16_t& y) {
  int16_t mx = 0, my = 0;
  const bool down = gt911::read(mx, my);
  switch (rotation & 3) {
    case 0: x = mx; y = my; break;
    case 1: x = PANEL_H - 1 - my; y = mx; break;
    case 2: x = PANEL_W - 1 - mx; y = PANEL_H - 1 - my; break;
    default: x = my; y = PANEL_W - 1 - mx; break;
  }
  return down;
}

bool canRotate() { return true; }

void setRotation(uint8_t r) {
  const uint8_t next = rc::rotationFromIndex(r);
  if (next != rotation) rgb800::clear();
  rotation = next;
}

bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }

bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

bool hasSdSlot() { return false; }
fs::FS* sdMount() { return nullptr; }
void sdUnmount() {}

}  // namespace board

#endif
