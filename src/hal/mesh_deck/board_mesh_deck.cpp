#if defined(BOARD_MESH_DECK)

#include <Arduino.h>
#include <Wire.h>

#include "hal/attaky/attaky_core.h"
#include "hal/attaky/pins_attaky_core.h"
#include "hal/board.h"
#include "hal/m9/panel_st7789.h"
#include "hal/mesh_deck/io_mesh_deck.h"
#include "hal/mesh_deck/pins_mesh_deck.h"
#include "input/keymap_mesh_deck.h"
#include "input/touch_transform.h"

namespace board {
namespace {

constexpr uint32_t kProbeMs = 1500;  // keyboard halves can be plugged in later

rc::MeshDeckKeys keys;
rc::MeshDeckPad pad;
rc::KeyEvent pending[8];
int pendingCount = 0;
uint32_t probeMs = 0;

void push(const rc::KeyEvent& e) {
  if (e.action != rc::Action::None && pendingCount < 8) pending[pendingCount++] = e;
}

void scan() {
  const uint32_t now = millis();
  if (now - probeMs >= kProbeMs) {
    probeMs = now;
    attaky_core::keyboardProbe();
  }
  attaky_core::KeyChange ch[8];
  const int n = attaky_core::keyboardScan(ch, 8);
  for (int i = 0; i < n; ++i) push(keys.key(ch[i].pos, ch[i].pressed, now));
  push(keys.poll(now));
  uint8_t pressed;
  if (attaky_core::readButtons(pressed)) {
    rc::KeyEvent ev[4];
    const int m = pad.update(pressed, now, ev, 4);
    for (int i = 0; i < m; ++i) push(ev[i]);
  }
}

}  // namespace

void begin() {
  Serial.printf("[INFO] [APP] Monster for Cyberspace v%s on Mesh Deck 1.0\n", RC_VERSION);
  Serial.printf("[INFO] [APP] Build: %s %s / toolchain: PlatformIO\n", __DATE__, __TIME__);
  Serial.println(mesh_deck::radioSleep() ? "[meshdeck] LoRa radio asleep" : "[meshdeck] no LoRa radio");
  st7789::begin({attaky_core::TFT_SCK, attaky_core::TFT_MOSI, -1, attaky_core::TFT_CS, attaky_core::TFT_DC,
                 attaky_core::TFT_RST, attaky_core::TFT_SPI_HZ, attaky_core::PANEL_MADCTL, true});
  ledcAttach(attaky_core::TFT_BL, attaky_core::TFT_BL_FREQ, 8);
  backlight(false);
  attaky_core::ioBegin("meshdeck");
  attaky_core::keyboardProbe();
  probeMs = millis();
  if (!attaky_core::keyboardPresent()) Serial.println("[meshdeck] no keyboard module");
}

const char* name() { return "MeshDeck"; }
const char* otaSlug() { return "mesh-deck"; }
int panelWidth() { return attaky_core::PANEL_W; }
int panelHeight() { return attaky_core::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, attaky_core::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(attaky_core::TFT_BL, level); }

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
  if (!pendingCount) scan();
  if (!pendingCount) return false;
  key = pending[0];
  for (int i = 1; i < pendingCount; ++i) pending[i - 1] = pending[i];
  --pendingCount;
  return true;
}

int batteryPercent() { return attaky_core::batteryPercent(); }

bool externalPower() { return attaky_core::externalPower(); }

void beep(int, int) {}  // no buzzer

const char* wakeHint() { return "press R2 to wake"; }

bool hasTouch() { return true; }
bool hasKeyboard() { return attaky_core::keyboardPresent(); }
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return false; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return true; }
void notifyLight(uint8_t rgb) { attaky_core::setLed(rgb); }
bool eink() { return false; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!attaky_core::readTouch(rx, ry)) return false;
  rc::touchToFrame(rx, ry, attaky_core::PANEL_NATIVE_W, attaky_core::PANEL_NATIVE_H, attaky_core::TOUCH_ROT,
                   x, y);
  return true;
}

bool canRotate() { return false; }
void setRotation(uint8_t) {}

// ATGM336H, powered with the board: "off" only stops the UART.
namespace {
bool gpsOn = false;
}

bool hasGps() { return true; }

void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    Serial1.setRxBufferSize(4096);
    Serial1.begin(mesh_deck::GPS_BAUD, SERIAL_8N1, mesh_deck::GPS_RX, mesh_deck::GPS_TX);
  } else {
    Serial1.end();
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < size_t(avail) ? n : size_t(avail));
}

// No RTC chip, no SD slot.
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }
bool hasSdSlot() { return false; }
fs::FS* sdMount() { return nullptr; }
void sdUnmount() {}

}  // namespace board

#endif
