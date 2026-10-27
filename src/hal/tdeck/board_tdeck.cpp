#if defined(BOARD_TDECK)

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/m9/panel_st7789.h"
#include "hal/tdeck/keyboard_tdeck.h"
#include "hal/tdeck/pins_tdeck.h"
#include "hal/tdeck/trackball_tdeck.h"
#include "hal/wio_l2/touch_gt911.h"
#include "input/keymap_tdeck.h"
#include "input/touch_transform.h"

namespace board {

void begin() {
  pinMode(tdeck::POWERON, OUTPUT);
  digitalWrite(tdeck::POWERON, HIGH);
  // Every device on the shared bus deselected before any of them is spoken to.
  for (int cs : {tdeck::LORA_CS, tdeck::SD_CS, tdeck::TFT_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  st7789::begin({tdeck::SPI_SCK, tdeck::SPI_MOSI, tdeck::SPI_MISO, tdeck::TFT_CS, tdeck::TFT_DC,
                 tdeck::TFT_RST, tdeck::TFT_SPI_HZ, tdeck::PANEL_MADCTL, true});
  ledcAttach(tdeck::TFT_BL, tdeck::TFT_BL_FREQ, 8);
  backlight(false);
  Wire.begin(tdeck::I2C_SDA, tdeck::I2C_SCL);
  tdeck_kb::begin();
  trackball::begin();
  // Only the UI task uses Wire on this board, so no lock.
  gt911::begin({&Wire, tdeck::TOUCH_ADDR, nullptr, nullptr});
}

const char* name() { return "TDeck"; }
int panelWidth() { return tdeck::PANEL_W; }
int panelHeight() { return tdeck::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, tdeck::PANEL_W, y0, y1);
}

void backlight(bool on) { ledcWrite(tdeck::TFT_BL, on ? 255 : 0); }

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    st7789::sleep(true);
  } else {
    st7789::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent& key) {
  uint8_t raw;
  if (tdeck_kb::poll(raw)) {
    key = rc::mapTdeckKey(raw);
    if (key.action != rc::Action::None) return true;
  }
  return trackball::poll(key);
}

int batteryPercent() { return -1; }
bool externalPower() { return false; }
void beep(int, int) {}

const char* wakeHint() { return "press any key to wake"; }

bool hasTouch() { return gt911::present(); }
bool hasKeyboard() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  const bool down = gt911::read(rx, ry);
  rc::touchToFrame(rx, ry, tdeck::PANEL_NATIVE_W, tdeck::PANEL_NATIVE_H, tdeck::TOUCH_ROT, x, y);
  return down;
}

bool canRotate() { return false; }
void setRotation(uint8_t) {}

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
