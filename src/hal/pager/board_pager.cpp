#if defined(BOARD_PAGER)

#include <Arduino.h>
#include <HWCDC.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/m9/panel_st7789.h"
#include "hal/pager/io_pager.h"
#include "hal/pager/keyboard_pager.h"
#include "hal/pager/pins_pager.h"
#include "hal/pager/wheel_pager.h"
#include "hal/wio_l2/audio_es8311.h"
#include "power/battery.h"

namespace board {
namespace {

// ST7796 power and gamma settings, from LovyanGFX's Panel_ST7796 (the driver
// camillia-mt runs this panel with).
constexpr uint8_t kSt7796Init[] = {
    0xF0, 1, 0xC3,  // command set control: unlock
    0xF0, 1, 0x96,
    0xB4, 1, 0x01,  // 1-dot inversion
    0xB6, 3, 0x80, 0x22, 0x3B,
    0xE8, 8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33,
    0xC1, 1, 0x06,
    0xC2, 1, 0xA7,
    0xC5, 1, 0x18,
    0xE0, 14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B,
    0xE1, 14, 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B,
    0xF0, 1, 0x3C,  // lock again
    0xF0, 1, 0x69,
    0xFF,
};

// BQ25896 charger: battery voltage from one-shot ADC conversions, started on
// one sample and read on the next (camillia-mt's battery_util.cpp).
constexpr uint8_t kRegAdc = 0x02, kRegWatchdog = 0x03, kRegStatus = 0x0B, kRegVbat = 0x0E;
constexpr uint32_t kChargerMs = 2000;
bool converting = false;
uint32_t chargerMs = 0;
bool chargerRead = false;
int percent = -1;
bool usb = false;

void chargerSample() {
  const uint32_t now = millis();
  if (chargerRead && now - chargerMs < kChargerMs) return;
  chargerRead = true;
  chargerMs = now;
  using pager::CHARGER_ADDR;
  pager::I2cGuard g;
  uint8_t r;
  // Its I2C watchdog (40 s) would put every register back to defaults.
  if (!pager::i2cRead(CHARGER_ADDR, kRegWatchdog, &r, 1)) {
    percent = -1;
    converting = false;
    return;
  }
  pager::i2cWrite(CHARGER_ADDR, kRegWatchdog, r | 0x40);
  // VBUS_STAT: anything but 0 is an adapter or a USB host.
  if (pager::i2cRead(CHARGER_ADDR, kRegStatus, &r, 1)) usb = ((r >> 5) & 7) != 0;
  if (!pager::i2cRead(CHARGER_ADDR, kRegAdc, &r, 1)) return;
  if (converting && !(r & 0x80)) {
    converting = false;
    uint8_t v;
    // 2.304 V + 20 mV a step; bit 7 is THERM_STAT. 0 means below the floor or
    // nothing measured, so keep what was shown.
    if (pager::i2cRead(CHARGER_ADDR, kRegVbat, &v, 1) && (v & 0x7F))
      percent = rc::batteryPercentFromVolts((2304 + 20 * (v & 0x7F)) / 1000.0f);
  }
  if (!converting) {
    r = (uint8_t)((r & ~0x40) | 0x80);  // one shot, start
    converting = pager::i2cWrite(CHARGER_ADDR, kRegAdc, r);
  }
}

}  // namespace

void begin() {
  // Every device on the shared SPI bus deselected before any of them is spoken to.
  for (int cs : {pager::LORA_CS, pager::SD_CS, pager::NFC_CS, pager::TFT_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  // The keyboard, LoRa and SD rails are on the expander, so it comes first.
  pager::ioBegin();
  st7789::Config panel{pager::SPI_SCK, pager::SPI_MOSI, pager::SPI_MISO, pager::TFT_CS,
                       pager::TFT_DC,  pager::TFT_RST,  pager::TFT_SPI_HZ, pager::PANEL_MADCTL,
                       true};
  panel.rowOffset = pager::PANEL_ROW_OFFSET;
  panel.init = kSt7796Init;
  st7789::begin(panel);
  ledcAttach(pager::TFT_BL, pager::TFT_BL_FREQ, 8);
  backlight(false);
  pager_kb::begin();
  wheel::begin();
  chargerSample();
  es8311::begin({pager::CODEC_ADDR, pager::I2S_MCLK, pager::I2S_BCK, pager::I2S_WS,
                 pager::I2S_DOUT, pager::AMP_SETTLE_MS,
                 [](bool on) { pager::ioSet(pager::EXP_AMP_EN, on); }, pager::i2cLock,
                 pager::i2cUnlock});
}

const char* name() { return "Pager"; }
const char* otaSlug() { return "pager"; }
int panelWidth() { return pager::PANEL_W; }
int panelHeight() { return pager::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, pager::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(pager::TFT_BL, level); }

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    st7789::sleep(true);
  } else {
    st7789::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent& key) { return pager_kb::poll(key) || wheel::poll(key); }

int batteryPercent() {
  chargerSample();
  return percent;
}

bool externalPower() {
  chargerSample();
  return usb || HWCDC::isPlugged();
}

void beep(int freqHz, int ms) { es8311::beep(freqHz, ms); }

const char* wakeHint() { return "press any key to wake"; }

bool hasTouch() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }
bool hasKeyboard() { return true; }
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }
bool upDownOnly() { return true; }
bool eink() { return false; }
bool doubleTapWakes() { return false; }

bool canRotate() { return false; }
void setRotation(uint8_t) {}

}  // namespace board

#endif
