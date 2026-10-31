#if defined(BOARD_X4)

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstring>

#include "hal/board.h"
#include "hal/x4/pins_x4.h"
#include "input/keymap_x4.h"
#include "power/battery.h"
#include "ui/eink.h"

// Xteink X4 (ESP32-C3, no PSRAM), from rhino-moshi's x4.h, panel_x4.cpp,
// buttons.cpp and power.cpp. There is no RAM for a 48 KB 1-bit frame next to
// Wi-Fi and TLS, so the panel's own RAM is the frame: at refresh time the UI
// redraws every line (board::setRedraw), and each 8 portrait lines, packed and
// turned to the panel's native landscape, go to the panel as one 480-byte
// column. rc::EinkPolicy says when.

namespace board {
namespace {

using namespace x4;

constexpr int kBands = NATIVE_W / 8;  // native byte columns, 8 portrait lines each

class X4Panel : public GxEPD2_426_GDEQ0426T82 {
 public:
  using Base = GxEPD2_426_GDEQ0426T82;
  using Base::Base;

  // The driver's first write clears the panel with a full refresh; do that
  // up front, outside a pass.
  void ready() {
    if (!_init_display_done || _initial_write) writeScreenBuffer();
  }
  // One native byte column, all NATIVE_H rows, to RAM ram (0x24 new, 0x26
  // old), in one burst (GxEPD2 clocks a byte at a time). Addressed as the
  // driver's _setPartialRamArea does: y counts down.
  void writeBand(uint8_t ram, int band, const uint8_t* bits) {
    const int x0 = band * 8, x1 = x0 + 7;
    _writeCommand(0x11);
    _writeData(0x01);
    _writeCommand(0x44);
    _writeData(x0 % 256);
    _writeData(x0 / 256);
    _writeData(x1 % 256);
    _writeData(x1 / 256);
    _writeCommand(0x45);
    _writeData((HEIGHT - 1) % 256);
    _writeData((HEIGHT - 1) / 256);
    _writeData(0);
    _writeData(0);
    _writeCommand(0x4e);
    _writeData(x0 % 256);
    _writeData(x0 / 256);
    _writeCommand(0x4f);
    _writeData((HEIGHT - 1) % 256);
    _writeData((HEIGHT - 1) / 256);
    _writeCommand(ram);
    _startTransfer();
    _pSPIx->writeBytes(bits, HEIGHT);
    _endTransfer();
  }
};

// The driver alone: no GxEPD2_BW buffer.
X4Panel epd(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
void (*redraw)() = nullptr;
rc::EinkPolicy policy;
bool sleeping = false;

// What the panel shows, as a hash per band: enough to see "nothing changed"
// and "a new screen".
uint32_t shownHash[kBands] = {};
uint32_t lastRefreshMs = 0;
// New content with no key press behind it redraws at most this often (spec §4.6).
constexpr uint32_t kPassiveGapMs = 2000;

// A pass: redraw's lines, packed a band at a time and written to the RAMs in
// passRams (bit 0: 0x24, bit 1: 0x26), hashed into passHash.
bool passing = false;
uint8_t passRams = 0;
uint32_t passHash[kBands];
uint8_t band[NATIVE_H];  // native rows, 1 = white
int bandAt = 0;          // the band being packed; bands below it are written

void flushBand() {
  uint32_t h = 2166136261u;
  for (int i = 0; i < NATIVE_H; ++i) h = (h ^ band[i]) * 16777619u;
  passHash[bandAt] = h;
  if (passRams & 1) epd.writeBand(0x24, bandAt, band);
  if (passRams & 2) epd.writeBand(0x26, bandAt, band);
  memset(band, 0xFF, sizeof band);
  ++bandAt;
}

// Portrait (x, y) is native (y, NATIVE_H - 1 - x): GxEPD2's rotation 3.
void packLine(const uint16_t* src, int y) {
  const int b = y >> 3;
  if (b < bandAt || b >= kBands) return;  // out of order: skipped
  while (bandAt < b) flushBand();         // lines never drawn stay white
  const uint8_t bit = 0x80 >> (y & 7);
  for (int x = 0; x < PANEL_W; ++x) {
    uint8_t& v = band[NATIVE_H - 1 - x];
    if (rc::rgb565Dark(src[x])) v &= ~bit;
    else v |= bit;
  }
}

void pass(uint8_t rams) {
  passing = true;
  passRams = rams;
  bandAt = 0;
  memset(band, 0xFF, sizeof band);
  redraw();
  while (bandAt < kBands) flushBand();
  passing = false;
}

int changedBands() {
  int diff = 0;
  for (int c = 0; c < kBands; ++c) diff += passHash[c] != shownHash[c];
  return diff;
}

volatile uint32_t lastInput = 0;

// Pass 1 (new image to 0x24, hashed) has run. Full: old = new too, then a
// full refresh. Partial: refresh, then old = new for the next one.
void refresh(bool full) {
  if (full) {
    pass(2);
    epd.refresh(false);
  } else {
    epd.refresh(true);
    pass(3);
  }
  epd.powerOff();
  memcpy(shownHash, passHash, sizeof shownHash);
  lastRefreshMs = millis();
}

void serviceDisplay() {
  if (!redraw || sleeping || !policy.pending()) return;
  const uint32_t now = millis();
  if (now - lastInput > rc::kEinkTypingMaxMs && now - lastRefreshMs < kPassiveGapMs) return;
  // A big change only turns a partial into a full: check before drawing.
  if (policy.due(now, lastInput, false) == rc::EinkRefresh::None) return;
  epd.ready();
  pass(1);
  const int diff = changedBands();
  const rc::EinkRefresh r = policy.due(now, lastInput, diff * 3 > kBands);
  if (r == rc::EinkRefresh::Partial && diff == 0) {
    policy.drop();
    return;
  }
  refresh(r == rc::EinkRefresh::Full);
  policy.sent(r, millis());
}

// Buttons: sampled every 5 ms by their own task, so a press isn't missed
// while the UI waits on the panel; keys are queued for pollKey.
QueueHandle_t keyQueue = nullptr;

int readAdc16(int pin) { return analogRead(pin) << 4; }  // 12-bit to the ladder's 16-bit units

void buttonTask(void*) {
  rc::X4Keys keys;
  rc::KeyEvent out[4];
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const uint8_t raw = rc::x4ButtonMask(readAdc16(BUTTON_ADC_1), readAdc16(BUTTON_ADC_2),
                                         digitalRead(BUTTON_POWER) == LOW);
    const uint32_t now = millis();
    if (raw) lastInput = now;
    const int n = keys.update(raw, now, out, 4);
    for (int i = 0; i < n; ++i) xQueueSend(keyQueue, &out[i], 0);
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
  }
}

// Battery: GPIO0 through a 2:1 divider, smoothed (rhino-moshi's power.cpp).
constexpr uint32_t kSampleMs = 2000;
constexpr float kSmoothing = 0.25f;
float volts = 0;
int samples = 0;
uint32_t sampleMs = 0;

void sampleBattery() {
  const uint32_t now = millis();
  if (samples && now - sampleMs < kSampleMs) return;
  sampleMs = now;
  uint32_t mv = 0;
  for (int i = 0; i < 8; ++i) mv += analogReadMilliVolts(BATTERY_ADC);
  const float v = mv / 8.0f * BATTERY_DIVIDER / 1000.0f;
  volts = samples ? volts + (v - volts) * kSmoothing : v;
  ++samples;
}

void holdLatch(bool on) {
  gpio_hold_dis(static_cast<gpio_num_t>(POWER_LATCH));
  pinMode(POWER_LATCH, OUTPUT);
  digitalWrite(POWER_LATCH, on ? HIGH : LOW);
  gpio_hold_en(static_cast<gpio_num_t>(POWER_LATCH));
}

[[noreturn]] void sleepNow(uint64_t timerUs) {
  epd.hibernate();
  Serial.flush();
  // Wait for the power button to come up, or it wakes the board at once.
  while (digitalRead(BUTTON_POWER) == LOW) delay(20);
  delay(50);
  Serial.end();
  if (timerUs) esp_sleep_enable_timer_wakeup(timerUs);
  // The C3 has no ext0/ext1; GPIO wake is what it offers.
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_POWER, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_config_gpio_isolate();
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
  esp_restart();  // only if sleep entry was refused
}

}  // namespace

void begin() {
  // A power-off holds the latch LOW through sleep; release that first.
  holdLatch(true);
  pinMode(USB_DETECT, INPUT);
  pinMode(BUTTON_POWER, INPUT_PULLUP);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  analogSetAttenuation(ADC_11db);

  // GxEPD2 writes these before setting their mode, which logs a warning.
  for (int p : {EPD_CS, EPD_DC, EPD_RST}) {
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  // 0: no GxEPD2 diagnostics (it would call Serial.begin again).
  epd.selectSPI(SPI, SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
  epd.init(0, true, 10, false);
  policy.changed(millis(), true);

  keyQueue = xQueueCreate(16, sizeof(rc::KeyEvent));
  xTaskCreate(buttonTask, "x4keys", 2048, nullptr, 3, nullptr);
  sampleBattery();
}

const char* name() { return "X4"; }
const char* otaSlug() { return "x4"; }
int panelWidth() { return PANEL_W; }
int panelHeight() { return PANEL_H; }

bool setRedraw(void (*fn)()) {
  redraw = fn;
  return true;
}

// Outside a pass, only which rows changed (frame is null). In a pass, frame
// points at row 0 of the full frame and only rows [y0, y1) are valid (the
// renderer's strip).
void pushRows(const uint16_t* frame, int y0, int y1) {
  if (y1 <= y0) return;
  if (!passing || !frame) {
    policy.changed(millis());
    return;
  }
  for (int y = y0; y < y1; ++y) packLine(frame + size_t(y) * PANEL_W, y);
}

void backlight(bool) {}
void backlightLevel(uint8_t) {}

// As on the T-Deck Pro: sleeping sends what is drawn at once, full; waking
// redraws full.
void displaySleep(bool sleep) {
  if (sleep == sleeping) return;
  if (sleep) {
    if (redraw && policy.pending()) {
      epd.ready();
      pass(1);
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
  return keyQueue && xQueueReceive(keyQueue, &key, 0) == pdTRUE;
}

int batteryPercent() {
  sampleBattery();
  return volts < 2.5f ? -1 : rc::batteryPercentFromVolts(volts);
}

bool externalPower() { return digitalRead(USB_DETECT) == HIGH; }

float batteryVolts() {
  sampleBattery();
  return volts;
}

void beep(int, int) {}
const char* wakeHint() { return "press Power to wake"; }

bool hasTouch() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }
bool hasKeyboard() { return false; }
bool hasKeys() { return true; }
bool backspaceIsBack() { return false; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return false; }
void notifyLight(uint8_t) {}
bool eink() { return true; }
bool canRotate() { return false; }
void setRotation(uint8_t) {}

bool canType() { return false; }
bool webCompose() { return true; }
bool sleepsOnBattery() { return true; }

void deepSleep(uint64_t timerUs) {
  holdLatch(true);  // stay connected to the battery through sleep
  sleepNow(timerUs);
}

bool wokeByTimer() { return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER; }

// Cuts the battery: on battery the X4 is off and the power button cold-boots
// it. On USB it stays in deep sleep until the power button.
void powerOff() {
  holdLatch(false);
  sleepNow(0);
}

bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

bool hasSdSlot() { return true; }

// On the panel's SPI bus. Slower clocks are tried for cards that won't start.
fs::FS* sdMount() {
  for (uint32_t hz : {4000000u, 1000000u}) {
    // Two files open at most (a folder and an entry): each costs ~4.6 KB.
    if (SD.begin(SD_CS, SPI, hz, "/sd", 2) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
