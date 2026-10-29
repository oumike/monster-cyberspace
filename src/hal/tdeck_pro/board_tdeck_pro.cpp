#if defined(BOARD_TDECK_PRO)

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <HWCDC.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#include <cstring>

#include "hal/board.h"
#include "hal/tdeck_pro/io_tdeck_pro.h"
#include "hal/tdeck_pro/keyboard_tdeck_pro.h"
#include "hal/tdeck_pro/pins_tdeck_pro.h"
#include "hal/tdeck_pro/touch_tdeck_pro.h"
#include "power/battery.h"
#include "ui/eink.h"

// E-paper handling from platy-aprs's display_eink.cpp and camillia-mt's
// tdeck_pro_display.h: the UI draws RGB565 as on every board, packed here to
// 1 bit and sent to the panel by rc::EinkPolicy's timing.

namespace board {
namespace {

using namespace tdeck_pro;

constexpr size_t kMonoBytes = size_t(PANEL_W + 7) / 8 * PANEL_H;

GxEPD2_BW<GxEPD2_310_GDEQ031T10, 40> epd(GxEPD2_310_GDEQ031T10(EPD_CS, EPD_DC, EPD_RST,
                                                               EPD_BUSY));
uint8_t* mono = nullptr;  // what the UI drew, 1 = white
uint8_t* shown = nullptr;  // what the panel shows
rc::EinkPolicy policy;
bool sleeping = false;

uint8_t* allocMono() {
  auto* p = static_cast<uint8_t*>(heap_caps_malloc(kMonoBytes, MALLOC_CAP_SPIRAM));
  if (!p) p = static_cast<uint8_t*>(malloc(kMonoBytes));
  if (p) memset(p, 0xFF, kMonoBytes);
  return p;
}

// GxEPD2 waits on BUSY for about 0.7 s (partial) or 1.1 s (full); the
// keyboard's FIFO is drained meanwhile so fast typing isn't lost.
void busyWait(const void*) {
  tdeck_pro_kb::service();
  delay(1);
}

// A new screen rather than a few changed characters: over a third differs.
bool bigChange() {
  size_t diff = 0;
  for (size_t i = 0; i < kMonoBytes; ++i) diff += mono[i] != shown[i];
  return diff * 3 > kMonoBytes;
}

void refresh(bool full) {
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  if (full) epd.setFullWindow();
  else epd.setPartialWindow(0, 0, PANEL_W, PANEL_H);
  epd.firstPage();
  do {
    epd.drawInvertedBitmap(0, 0, mono, PANEL_W, PANEL_H, GxEPD_BLACK);
  } while (epd.nextPage());
  epd.powerOff();
  memcpy(shown, mono, kMonoBytes);
}

void serviceDisplay() {
  if (!mono || !shown || sleeping || !policy.pending()) return;
  const uint32_t now = millis();
  const rc::EinkRefresh r = policy.due(now, tdeck_pro_kb::lastInputMs(), bigChange());
  if (r == rc::EinkRefresh::None) return;
  if (r == rc::EinkRefresh::Partial && memcmp(mono, shown, kMonoBytes) == 0) {
    policy.drop();
    return;
  }
  refresh(r == rc::EinkRefresh::Full);
  policy.sent(r, millis());
}

// The side button: a press sends ScreenOff, which locks the screen or, when
// it is dark, wakes it.
constexpr uint32_t kButtonDebounceMs = 30;
bool buttonDown = false, buttonRaw = false;
uint32_t buttonChangedMs = 0;

bool pollButton(rc::KeyEvent& key) {
  const uint32_t now = millis();
  const bool raw = digitalRead(SIDE_BUTTON) == LOW;
  if (raw != buttonRaw) {
    buttonRaw = raw;
    buttonChangedMs = now;
  }
  if (raw == buttonDown || now - buttonChangedMs < kButtonDebounceMs) return false;
  buttonDown = raw;
  if (!raw) return false;
  key = rc::KeyEvent{rc::Action::ScreenOff};
  return true;
}

// BQ27220 gauge first; the BQ25896 charger's battery voltage when the gauge
// doesn't answer (camillia-mt's battery_util.cpp, as on the T-LoRa Pager).
constexpr uint8_t kRegAdc = 0x02, kRegWatchdog = 0x03, kRegStatus = 0x0B, kRegVbat = 0x0E;
constexpr uint8_t kGaugeSoc = 0x2C;
constexpr uint32_t kChargerMs = 2000;
bool converting = false, chargerRead = false, gaugeFailed = false;
uint32_t chargerMs = 0;
int percent = -1;
bool usb = false;

void chargerSample() {
  const uint32_t now = millis();
  if (chargerRead && now - chargerMs < kChargerMs) return;
  chargerRead = true;
  chargerMs = now;
  I2cGuard g;
  uint8_t r;
  // Its I2C watchdog (40 s) would put every register back to defaults.
  if (!i2cRead(CHARGER_ADDR, kRegWatchdog, &r, 1)) {
    converting = false;
  } else {
    i2cWrite(CHARGER_ADDR, kRegWatchdog, r | 0x40);
    if (i2cRead(CHARGER_ADDR, kRegStatus, &r, 1)) usb = ((r >> 5) & 7) != 0;
    if (gaugeFailed && i2cRead(CHARGER_ADDR, kRegAdc, &r, 1)) {
      if (converting && !(r & 0x80)) {
        converting = false;
        uint8_t v;
        if (i2cRead(CHARGER_ADDR, kRegVbat, &v, 1) && (v & 0x7F))
          percent = rc::batteryPercentFromVolts((2304 + 20 * (v & 0x7F)) / 1000.0f);
      }
      if (!converting) converting = i2cWrite(CHARGER_ADDR, kRegAdc, uint8_t((r & ~0x40) | 0x80));
    }
  }
  if (gaugeFailed) return;
  uint8_t v[2];
  if (!i2cRead(GAUGE_ADDR, kGaugeSoc, v, 2)) {
    gaugeFailed = true;
    percent = -1;
    Serial.println("[bat] BQ27220 not answering; battery from voltage");
    return;
  }
  const int soc = v[0] | v[1] << 8;
  percent = soc <= 100 ? soc : -1;
}

}  // namespace

void begin() {
  // Everything on the shared SPI bus deselected before any of it is spoken to.
  for (int cs : {EPD_CS, LORA_CS, SD_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  pinMode(LORA_POWER, OUTPUT);
  digitalWrite(LORA_POWER, LOW);  // the radio isn't used
  pinMode(GPS_EN, OUTPUT);
  digitalWrite(GPS_EN, LOW);  // until gpsPower()
  pinMode(SIDE_BUTTON, INPUT_PULLUP);
  // E-paper is read by daylight: the frontlight stays off.
  pinMode(FRONTLIGHT, OUTPUT);
  digitalWrite(FRONTLIGHT, LOW);

  mono = allocMono();
  shown = allocMono();
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  // 0: no GxEPD2 diagnostics (it would call Serial.begin again).
  epd.init(0, true, 2, false, SPI, SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
  epd.setRotation(0);
  epd.epd2.setBusyCallback(busyWait);
  policy.changed(millis(), true);

  i2cBegin();
  tdeck_pro_kb::begin();
  tdeck_pro_touch::begin();
  chargerSample();
}

const char* name() { return "TDeckPro"; }
const char* otaSlug() { return "tdeck-pro"; }
int panelWidth() { return PANEL_W; }
int panelHeight() { return PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  if (!mono || y1 <= y0) return;
  rc::packMono(frame, PANEL_W, y0, y1, mono);
  policy.changed(millis());
}

// No light to dim; the panel keeps its image unpowered.
void backlight(bool) {}
void backlightLevel(uint8_t) {}

// Sleeping sends what is drawn (the screensaver) at once with a full refresh,
// then stops refreshing. Waking redraws with a full refresh.
void displaySleep(bool sleep) {
  if (sleep == sleeping) return;
  if (sleep) {
    if (mono && shown && policy.pending()) {
      refresh(true);
      policy.sent(rc::EinkRefresh::Full, millis());
    }
    sleeping = true;
  } else {
    sleeping = false;
    policy.changed(millis(), true);
  }
}

bool pollKey(rc::KeyEvent& key) {
  serviceDisplay();
  return pollButton(key) || tdeck_pro_kb::poll(key);
}

int batteryPercent() {
  chargerSample();
  return percent;
}

bool externalPower() {
  chargerSample();
  return usb || HWCDC::isPlugged();
}

void beep(int, int) {}

const char* wakeHint() { return "press the side button to wake"; }

bool hasTouch() { return tdeck_pro_touch::present(); }
bool readTouch(int16_t& x, int16_t& y) { return tdeck_pro_touch::read(x, y); }
bool hasKeyboard() { return true; }
// The keyboard has no good arrows: navigation is by touch, with the side strip.
bool hasKeys() { return false; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return true; }
void textEntry(bool) {}
bool eink() { return true; }
bool doubleTapWakes() { return false; }

bool canRotate() { return false; }
void setRotation(uint8_t) {}

}  // namespace board

#endif
