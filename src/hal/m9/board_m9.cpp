#if defined(BOARD_M9)

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "hal/board.h"
#include "hal/m9/keyboard_stc8h.h"
#include "hal/m9/panel_st7789.h"
#include "hal/m9/pins_m9.h"
#include "input/keymap_m9.h"
#include "power/battery.h"

namespace board {

void begin() {
  pinMode(m9::VEXT, OUTPUT);
  digitalWrite(m9::VEXT, LOW);  // rail on
  pinMode(m9::GPS_EN, OUTPUT);
  digitalWrite(m9::GPS_EN, HIGH);  // GPS off until the time service turns it on
  pinMode(m9::GPS_RST, OUTPUT);
  digitalWrite(m9::GPS_RST, LOW);
  delay(50);
  // Every device on the shared bus deselected before any of them is spoken to.
  for (int cs : {m9::SD_CS, m9::LORA_CS, m9::TFT_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  SPI.begin(m9::SPI_SCK, m9::SPI_MISO, m9::SPI_MOSI);
  // MADCTL 0xA0: landscape, keyboard below the screen. Inverted, as this panel needs.
  st7789::begin({m9::SPI_SCK, m9::SPI_MOSI, m9::SPI_MISO, m9::TFT_CS, m9::TFT_DC, m9::TFT_RST,
                 m9::TFT_SPI_HZ, 0xA0, true});
  // Channel 7, not 0: tone() allocates from 0 upward, and losing the drive on
  // this active-LOW pin would light the backlight.
  ledcAttachChannel(m9::TFT_BL, m9::TFT_BL_FREQ, 8, m9::TFT_BL_PWM_CH);
  backlight(false);
  analogReadResolution(12);
  analogSetPinAttenuation(m9::BATT_ADC, ADC_11db);
  stc8h::begin();
}

const char* name() { return "M9"; }
const char* otaSlug() { return "m9"; }
int panelWidth() { return m9::PANEL_W; }
int panelHeight() { return m9::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, m9::PANEL_W, y0, y1);
}

void backlight(bool on) { ledcWrite(m9::TFT_BL, on ? 0 : 255); }  // active LOW

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
  if (!stc8h::poll(raw)) return false;
  key = rc::mapM9Key(raw, stc8h::early());
  return true;
}

static float batteryVolts() {
  return analogReadMilliVolts(m9::BATT_ADC) * m9::BATT_DIV / 1000.0f;
}

int batteryPercent() { return rc::batteryPercentFromVolts(batteryVolts()); }

// No VBUS sense on this board: a cell held above its resting maximum means
// it's on the charger.
bool externalPower() { return batteryVolts() > 4.25f; }

void beep(int freqHz, int ms) { tone(m9::BUZZER, freqHz, ms); }

const char* wakeHint() { return "hold \xE2\x97\x8F to wake"; }

bool hasTouch() { return false; }
bool hasKeyboard() { return true; }
bool hasKeys() { return true; }
bool backspaceIsBack() { return false; }
bool anyInputWakes() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }
bool canRotate() { return false; }
void setRotation(uint8_t) {}

bool hasSdSlot() { return true; }

// Shared SPI bus, already set up in begin(). A card reporting CARD_NONE is
// treated as no card.
fs::FS* sdMount() {
  if (!SD.begin(m9::SD_CS, SPI, m9::SD_SPI_HZ)) return nullptr;
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    return nullptr;
  }
  return &SD;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
