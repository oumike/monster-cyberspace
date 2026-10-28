#if defined(BOARD_CROWPANEL_35)

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <utility>

#include "hal/board.h"
#include "hal/crowpanel/panel_ili9488.h"
#include "hal/crowpanel/pins_crowpanel.h"
#include "hal/wio_l2/touch_gt911.h"
#include "ui/rotation.h"

// Elecrow CrowPanel Advance 3.5": touch only, no keyboard or buttons. The
// screen stays lit (there is no button to turn it off) and a touch wakes it.

namespace board {
namespace {

using namespace crowpanel::p35;

uint8_t rotation = 0;
SPIClass sdSpi(FSPI);
bool gpsOn = false;

// INT held LOW through reset straps the GT911 to 0x5D.
void resetTouch() {
  pinMode(TOUCH_INT, OUTPUT);
  digitalWrite(TOUCH_INT, LOW);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(20);
  digitalWrite(TOUCH_RST, HIGH);
  delay(100);
  pinMode(TOUCH_INT, INPUT);
  delay(50);
}

}  // namespace

void begin() {
  ledcAttach(TFT_BL, TFT_BL_FREQ, 8);
  ledcWrite(TFT_BL, 0);
  if (!ili9488::begin()) Serial.println("[crowpanel] panel init failed");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  resetTouch();
  Wire.begin(crowpanel::I2C_SDA, crowpanel::I2C_SCL);
  // Only the UI task uses Wire on this board, so no lock.
  gt911::begin({&Wire, crowpanel::TOUCH_ADDR, nullptr, nullptr});
}

const char* name() { return "CrowPanel35"; }
const char* otaSlug() { return "crowpanel35"; }
int panelWidth() { return rc::rotationPortrait(rotation) ? PANEL_H : PANEL_W; }
int panelHeight() { return rc::rotationPortrait(rotation) ? PANEL_W : PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  ili9488::pushRows(frame, panelWidth(), y0, y1);
}

void backlight(bool on) { ledcWrite(TFT_BL, on ? BACKLIGHT_ON : 0); }

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    ili9488::sleep(true);
  } else {
    ili9488::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent&) { return false; }

// No battery sense: powered over USB or the battery connector's charger.
int batteryPercent() { return -1; }
bool externalPower() { return true; }

// GPIO8 drives the I2S amplifier's MCLK, not a buzzer: no beeps.
void beep(int, int) {}

const char* wakeHint() { return "tap to wake"; }

bool hasTouch() { return gt911::present(); }
bool hasKeyboard() { return false; }
bool hasKeys() { return false; }
bool backspaceIsBack() { return false; }
bool anyInputWakes() { return true; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx = 0, ry = 0;
  const bool down = gt911::read(rx, ry);
  if (rx < 0) rx = 0;
  if (rx >= PANEL_NATIVE_W) rx = PANEL_NATIVE_W - 1;
  if (ry < 0) ry = 0;
  if (ry >= PANEL_NATIVE_H) ry = PANEL_NATIVE_H - 1;
  // LovyanGFX's own touch transform (Panel_Device::convertRawXY) for the
  // panel's rotation, offset_rotation 3 plus ours: what camillia-mt gets.
  // rc::touchToFrame turns portrait the other way, so it isn't used here.
  const uint8_t r = (rotation + 3) & 3;
  int tx = rx, ty = ry;
  if (r & 1) std::swap(tx, ty);
  if (r & 2) tx = panelWidth() - 1 - tx;
  if (r == 1 || r == 2) ty = panelHeight() - 1 - ty;
  x = static_cast<int16_t>(tx);
  y = static_cast<int16_t>(ty);
  return down;
}

bool canRotate() { return true; }

void setRotation(uint8_t r) {
  rotation = rc::rotationFromIndex(r);
  ili9488::setRotation(rotation);
}

// Optional GPS module on the UART1 connector, powered with the board: "off"
// only stops the UART. Nothing answers if no module is fitted.
bool hasGps() { return true; }

void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    Serial1.setRxBufferSize(4096);
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  } else {
    Serial1.end();
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < static_cast<size_t>(avail) ? n : static_cast<size_t>(avail));
}

bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

bool hasSdSlot() { return true; }

// The card has SPI2 to itself (the panel is on SPI3). CARD_NONE is no card.
fs::FS* sdMount() {
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSpi, SD_SPI_HZ) || SD.cardType() == CARD_NONE) {
    SD.end();
    sdSpi.end();
    return nullptr;
  }
  return &SD;
}

void sdUnmount() {
  SD.end();
  sdSpi.end();
}

}  // namespace board

#endif
