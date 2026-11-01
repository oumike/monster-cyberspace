#if defined(BOARD_CARDPUTER_ADV)

#include <Arduino.h>
#include <HWCDC.h>

#include "hal/board.h"
#include "hal/cardputer_adv/keyboard_cardputer_adv.h"
#include "hal/cardputer_adv/pins_cardputer_adv.h"
#include "hal/m9/panel_st7789.h"
#include "hal/wio_l2/audio_es8311.h"
#include "power/battery.h"
#include "ui/grid.h"

// M5Stack Cardputer ADV (spec 2026-11-01-cyberspace-cardputer-adv-design.md §4).

namespace board {

void begin() {
  // A fitted LoRa cap's radio stays off the SD bus (pins_cardputer_adv.h).
  pinMode(cardputer::CAP_LORA_CS, OUTPUT);
  digitalWrite(cardputer::CAP_LORA_CS, HIGH);
  // The panel has the default SPI bus to itself (MISO unused).
  st7789::Config panel{cardputer::TFT_SCK, cardputer::TFT_MOSI, -1,
                       cardputer::TFT_CS,  cardputer::TFT_DC,   cardputer::TFT_RST,
                       cardputer::TFT_SPI_HZ, cardputer::PANEL_MADCTL, true};
  panel.colOffset = cardputer::PANEL_COL_OFFSET;
  panel.rowOffset = cardputer::PANEL_ROW_OFFSET;
  st7789::begin(panel);
  ledcAttach(cardputer::TFT_BL, cardputer::TFT_BL_FREQ, 8);
  backlight(false);
  analogReadResolution(12);
  analogSetPinAttenuation(cardputer::BATT_ADC, ADC_11db);
  cardputer_kb::begin();
  es8311::Config audio{cardputer::CODEC_ADDR, -1, cardputer::I2S_BCK, cardputer::I2S_WS,
                       cardputer::I2S_DOUT, 0, nullptr, cardputer_kb::i2cLock,
                       cardputer_kb::i2cUnlock};
  audio.mclkFromBclk = true;
  es8311::begin(audio);
}

const char* name() { return "Cardputer"; }
const char* otaSlug() { return "cardputer-adv"; }
int panelWidth() { return cardputer::PANEL_W; }
int panelHeight() { return cardputer::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, cardputer::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(cardputer::TFT_BL, level); }

void displaySleep(bool sleep) {
  if (sleep) {
    backlight(false);
    st7789::sleep(true);
  } else {
    st7789::sleep(false);
    backlight(true);
  }
}

bool pollKey(rc::KeyEvent& key) { return cardputer_kb::poll(key); }

float batteryVolts() {
  return analogReadMilliVolts(cardputer::BATT_ADC) * cardputer::BATT_DIV / 1000.0f;
}

int batteryPercent() { return rc::batteryPercentFromVolts(batteryVolts()); }

// No charger status line: USB attached, or a cell held above its resting
// maximum, means it's on the charger.
bool externalPower() { return HWCDC::isPlugged() || batteryVolts() > 4.25f; }

void beep(int freqHz, int ms) { es8311::beep(freqHz, ms); }

const char* wakeHint() { return "press any key to wake"; }

// 240x135: Compact (6x12) gives a 40x11 grid; anything bigger leaves too few rows.
bool fixedTextSize(rc::TextSize& out) {
  out = rc::TextSize::Compact;
  return true;
}

bool hasTouch() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }
bool hasKeyboard() { return true; }
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return false; }
void notifyLight(uint8_t) {}
bool eink() { return false; }
bool doubleTapWakes() { return false; }

bool canRotate() { return false; }
void setRotation(uint8_t) {}

bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

}  // namespace board

#endif
