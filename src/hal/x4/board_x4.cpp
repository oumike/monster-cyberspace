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
// buttons.cpp and power.cpp. The UI draws RGB565 strips (ui/renderer's strip
// mode); pushRows packs them, turned to the panel's native landscape, into
// one 48 KB 1-bit frame, which goes to the panel by rc::EinkPolicy's timing.

namespace board {
namespace {

using namespace x4;

constexpr int kRowBytes = NATIVE_W / 8;  // a native row
constexpr size_t kMonoBytes = size_t(kRowBytes) * NATIVE_H;

// GxEPD2 clocks a frame out a byte at a time (~170 ms for 48 KB); whole
// frames go out in one burst instead (rhino-moshi's panel_x4.cpp). Anything
// else, and the first write after init, falls through to the library.
class X4Panel : public GxEPD2_426_GDEQ0426T82 {
 public:
  using Base = GxEPD2_426_GDEQ0426T82;
  using Base::Base;

  void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                  bool mirror_y = false, bool pgm = false) {
    if (!fast(0x24, bitmap, x, y, w, h, invert, mirror_y, pgm))
      Base::writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
  void writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
                                bool invert = false, bool mirror_y = false, bool pgm = false) {
    if (fast(0x26, bitmap, x, y, w, h, invert, mirror_y, pgm)) fast(0x24, bitmap, x, y, w, h, invert, mirror_y, pgm);
    else Base::writeImageForFullRefresh(bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
  void writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false,
                       bool mirror_y = false, bool pgm = false) {
    if (fast(0x26, bitmap, x, y, w, h, invert, mirror_y, pgm)) fast(0x24, bitmap, x, y, w, h, invert, mirror_y, pgm);
    else Base::writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
  }

 private:
  bool fast(uint8_t ram, const uint8_t* bitmap, int16_t x, int16_t y, int16_t w, int16_t h, bool invert,
            bool mirror_y, bool pgm) {
    if (x || y || w != WIDTH || h != HEIGHT || invert || mirror_y || pgm || !_init_display_done || _initial_write)
      return false;
    // The whole RAM, as the driver's _setPartialRamArea(0, 0, WIDTH, HEIGHT) sets it: y counts down.
    _writeCommand(0x11);
    _writeData(0x01);
    _writeCommand(0x44);
    _writeData(0);
    _writeData(0);
    _writeData((WIDTH - 1) % 256);
    _writeData((WIDTH - 1) / 256);
    _writeCommand(0x45);
    _writeData((HEIGHT - 1) % 256);
    _writeData((HEIGHT - 1) / 256);
    _writeData(0);
    _writeData(0);
    _writeCommand(0x4e);
    _writeData(0);
    _writeData(0);
    _writeCommand(0x4f);
    _writeData((HEIGHT - 1) % 256);
    _writeData((HEIGHT - 1) / 256);
    _writeCommand(ram);
    _startTransfer();
    _pSPIx->writeBytes(bitmap, uint32_t(WIDTH) * HEIGHT / 8);
    _endTransfer();
    return true;
  }
};

// The driver alone: our frame is the buffer, so GxEPD2_BW's isn't needed.
X4Panel epd(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
uint8_t* mono = nullptr;  // native landscape, 1 = white
rc::EinkPolicy policy;
bool sleeping = false;

// What the panel shows, as a hash per native byte column (8 portrait rows
// each), instead of a second 48 KB frame: enough to see "nothing changed"
// and "a new screen".
uint32_t shownHash[kRowBytes] = {};
uint32_t lastRefreshMs = 0;
// New content with no key press behind it redraws at most this often (spec §4.6).
constexpr uint32_t kPassiveGapMs = 2000;

// Portrait (x, y) is native (y, NATIVE_H - 1 - x): GxEPD2's rotation 3.
void packRows(const uint16_t* frame, int y0, int y1) {
  for (int y = y0; y < y1; ++y) {
    const uint16_t* src = frame + size_t(y) * PANEL_W;
    const int col = y >> 3;
    const uint8_t bit = 0x80 >> (y & 7);
    for (int x = 0; x < PANEL_W; ++x) {
      uint8_t& b = mono[size_t(NATIVE_H - 1 - x) * kRowBytes + col];
      if (rc::rgb565Dark(src[x])) b &= ~bit;
      else b |= bit;
    }
  }
}

void columnHashes(uint32_t* out) {
  for (int c = 0; c < kRowBytes; ++c) out[c] = 2166136261u;
  for (int r = 0; r < NATIVE_H; ++r) {
    const uint8_t* row = mono + size_t(r) * kRowBytes;
    for (int c = 0; c < kRowBytes; ++c) out[c] = (out[c] ^ row[c]) * 16777619u;
  }
}

volatile uint32_t lastInput = 0;

void refresh(bool full) {
  if (full) {
    epd.writeImageForFullRefresh(mono, 0, 0, NATIVE_W, NATIVE_H);
    epd.refresh(false);
  } else {
    epd.writeImage(mono, 0, 0, NATIVE_W, NATIVE_H);
    epd.refresh(true);
  }
  epd.writeImageAgain(mono, 0, 0, NATIVE_W, NATIVE_H);
  epd.powerOff();
  lastRefreshMs = millis();
}

void serviceDisplay() {
  if (!mono || sleeping || !policy.pending()) return;
  const uint32_t now = millis();
  if (now - lastInput > rc::kEinkTypingMaxMs && now - lastRefreshMs < kPassiveGapMs) return;
  uint32_t hash[kRowBytes];
  columnHashes(hash);
  int diff = 0;
  for (int c = 0; c < kRowBytes; ++c) diff += hash[c] != shownHash[c];
  const rc::EinkRefresh r = policy.due(now, lastInput, diff * 3 > kRowBytes);
  if (r == rc::EinkRefresh::None) return;
  if (r == rc::EinkRefresh::Partial && diff == 0) {
    policy.drop();
    return;
  }
  refresh(r == rc::EinkRefresh::Full);
  memcpy(shownHash, hash, sizeof hash);
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

  mono = static_cast<uint8_t*>(malloc(kMonoBytes));
  if (mono) memset(mono, 0xFF, kMonoBytes);
  else Serial.println("[x4] no RAM for the e-paper frame");
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

// frame points at row 0 of the full frame; only rows [y0, y1) are valid (the
// renderer's strip).
void pushRows(const uint16_t* frame, int y0, int y1) {
  if (!mono || y1 <= y0) return;
  packRows(frame, y0, y1);
  policy.changed(millis());
}

void backlight(bool) {}
void backlightLevel(uint8_t) {}

// As on the T-Deck Pro: sleeping sends what is drawn at once, full; waking
// redraws full.
void displaySleep(bool sleep) {
  if (sleep == sleeping) return;
  if (sleep) {
    if (mono && policy.pending()) {
      refresh(true);
      columnHashes(shownHash);
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
    if (SD.begin(SD_CS, SPI, hz) && SD.cardType() != CARD_NONE) return &SD;
    SD.end();
  }
  return nullptr;
}

void sdUnmount() { SD.end(); }

}  // namespace board

#endif
