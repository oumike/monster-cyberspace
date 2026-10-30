#if defined(BOARD_MESH_DECK)

#include <Arduino.h>
#include <Wire.h>

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
    mesh_deck::keyboardProbe();
  }
  mesh_deck::KeyChange ch[8];
  const int n = mesh_deck::keyboardScan(ch, 8);
  for (int i = 0; i < n; ++i) push(keys.key(ch[i].pos, ch[i].pressed, now));
  push(keys.poll(now));
  uint8_t pressed;
  if (mesh_deck::readButtons(pressed)) {
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
  st7789::begin({mesh_deck::TFT_SCK, mesh_deck::TFT_MOSI, -1, mesh_deck::TFT_CS, mesh_deck::TFT_DC,
                 mesh_deck::TFT_RST, mesh_deck::TFT_SPI_HZ, mesh_deck::PANEL_MADCTL, true});
  ledcAttach(mesh_deck::TFT_BL, mesh_deck::TFT_BL_FREQ, 8);
  backlight(false);
  mesh_deck::ioBegin();
  mesh_deck::keyboardProbe();
  probeMs = millis();
  if (!mesh_deck::keyboardPresent()) Serial.println("[meshdeck] no keyboard module");
}

const char* name() { return "MeshDeck"; }
const char* otaSlug() { return "mesh-deck"; }
int panelWidth() { return mesh_deck::PANEL_W; }
int panelHeight() { return mesh_deck::PANEL_H; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  st7789::pushRows(frame, mesh_deck::PANEL_W, y0, y1);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { ledcWrite(mesh_deck::TFT_BL, level); }

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

int batteryPercent() { return mesh_deck::batteryPercent(); }

// No charger status line: the gauge's charge rate rising, or a cell held
// above its resting maximum, means USB power.
bool externalPower() {
  int16_t rate;
  if (mesh_deck::batteryRate(rate) && rate > 20) return true;
  return mesh_deck::batteryMv() > 4150;
}

void beep(int, int) {}  // no buzzer

const char* wakeHint() { return "press R2 to wake"; }

bool hasTouch() { return true; }
bool hasKeyboard() { return mesh_deck::keyboardPresent(); }
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
void notifyLight(uint8_t rgb) { mesh_deck::setLed(rgb); }
bool eink() { return false; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!mesh_deck::readTouch(rx, ry)) return false;
  rc::touchToFrame(rx, ry, mesh_deck::PANEL_NATIVE_W, mesh_deck::PANEL_NATIVE_H, mesh_deck::TOUCH_ROT,
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
