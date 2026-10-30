#if defined(BOARD_TDISPLAY_P4)

#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>

#include "hal/board.h"
#include "hal/tdisplay_p4/io_tdisplay_p4.h"
#include "hal/tdisplay_p4/panel_rm69a10.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"
#include "hal/tdisplay_p4/touch_gt9895.h"
#include "hal/wio_l2/audio_es8311.h"
#include "input/touch_transform.h"
#include "ui/rotation.h"

// LilyGO T-Display P4 (ESP32-P4, Wi-Fi through an ESP32-C6 over ESP-Hosted):
// touch only, plus a user button that locks and wakes the screen. Drawn at
// half resolution and doubled (pins_tdisplay_p4.h). The LoRa radio, the
// optional keyboard accessory, the RTC and Ethernet aren't used.

namespace board {
namespace {

using namespace tdisplay_p4;

constexpr int kFrameShort = PANEL_W / SCALE, kFrameLong = PANEL_H / SCALE;  // 284 x 616
uint8_t rotation = 0;

// The user button: a press sends ScreenOff, which locks the screen or, when
// it is dark, wakes it.
constexpr uint32_t kButtonDebounceMs = 30;
bool buttonDown = false, buttonRaw = false;
uint32_t buttonChangedMs = 0;

// BQ27220 gauge, sampled every few seconds.
constexpr uint8_t kGaugeStatus = 0x0A, kGaugeSoc = 0x2C;
constexpr uint32_t kGaugeMs = 5000;
uint32_t gaugeMs = 0;
bool gaugeRead = false, discharging = true;
int percent = -1;

bool gaugeReg(uint8_t reg, uint16_t& v) {
  Wire.beginTransmission(GAUGE_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(GAUGE_ADDR, uint8_t(2)) != 2) return false;
  v = uint16_t(Wire.read());
  v |= uint16_t(Wire.read()) << 8;
  return true;
}

void gaugeSample() {
  const uint32_t now = millis();
  if (gaugeRead && now - gaugeMs < kGaugeMs) return;
  gaugeRead = true;
  gaugeMs = now;
  I2cGuard g;
  uint16_t soc, status;
  percent = gaugeReg(kGaugeSoc, soc) && soc <= 100 ? soc : -1;
  if (gaugeReg(kGaugeStatus, status)) discharging = status & 1;  // DSG
}

}  // namespace

void begin() {
  installFullRestart();
  if (!ioBegin()) Serial.println("[p4] expander init failed");
  // Wi-Fi lives on the C6; start it from reset before anything asks for it.
  if (!resetC6()) Serial.println("[p4] ESP32-C6 reset failed");
  if (!p4panel::begin()) Serial.println("[p4] panel init failed");
  backlight(false);
  gt9895::begin();
  pinMode(USER_BUTTON, INPUT_PULLUP);
  Wire1.begin(AUDIO_SDA, AUDIO_SCL, 400000);
  es8311::begin({CODEC_ADDR, I2S_MCLK, I2S_BCK, I2S_WS, I2S_DOUT, AMP_SETTLE_MS,
                 [](bool on) { ioSet(EXP_AUDIO_POWER, on); }, nullptr, nullptr, &Wire1});
}

const char* name() { return "TDisplayP4"; }
const char* otaSlug() { return "tdisplay-p4"; }
int panelWidth() { return rc::rotationPortrait(rotation) ? kFrameShort : kFrameLong; }
int panelHeight() { return rc::rotationPortrait(rotation) ? kFrameLong : kFrameShort; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  p4panel::pushRows(frame, panelWidth(), panelHeight(), y0, y1, rotation);
}

void backlight(bool on) { backlightLevel(on ? 255 : 0); }
void backlightLevel(uint8_t level) { p4panel::brightness(level); }
// The AMOLED's pixels are its light: dark at brightness 0.
void displaySleep(bool sleep) { backlight(!sleep); }

bool pollKey(rc::KeyEvent& key) {
  const uint32_t now = millis();
  const bool raw = digitalRead(USER_BUTTON) == LOW;
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

int batteryPercent() {
  gaugeSample();
  return percent;
}

bool externalPower() {
  gaugeSample();
  return !discharging;
}

void beep(int freqHz, int ms) { es8311::beep(freqHz, ms); }

const char* wakeHint() { return "double-tap or press the button"; }

bool hasTouch() { return gt9895::present(); }
bool hasKeyboard() { return false; }
bool hasKeys() { return false; }
bool backspaceIsBack() { return false; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return true; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return false; }
void notifyLight(uint8_t) {}
bool eink() { return false; }

bool readTouch(int16_t& x, int16_t& y) {
  int16_t nx, ny;
  if (!gt9895::read(nx, ny)) return false;
  rc::touchToFrame(int16_t(nx / SCALE), int16_t(ny / SCALE), kFrameShort, kFrameLong, rotation, x, y);
  return true;
}

bool canRotate() { return true; }

void setRotation(uint8_t r) {
  const uint8_t next = rc::rotationFromIndex(r);
  if (next != rotation) p4panel::clear();
  rotation = next;
}

// L76K: woken through the expander, on UART1.
namespace {
bool gpsOn = false;
}

bool hasGps() { return true; }

void gpsPower(bool on) {
  if (on == gpsOn) return;
  if (on) {
    ioSet(EXP_GPS_WAKE, true);
    Serial1.setRxBufferSize(4096);
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  } else {
    Serial1.end();
    ioSet(EXP_GPS_WAKE, false);
  }
  gpsOn = on;
}

size_t gpsRead(uint8_t* buf, size_t n) {
  if (!gpsOn) return 0;
  const int avail = Serial1.available();
  if (avail <= 0) return 0;
  return Serial1.read(buf, n < size_t(avail) ? n : size_t(avail));
}

bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

bool hasSdSlot() { return true; }

fs::FS* sdMount() {
  ioSet(EXP_SD_POWER, false);  // active low: on
  delay(10);
  if (SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3) && SD_MMC.begin("/sdcard", false, false) &&
      SD_MMC.cardType() != CARD_NONE)
    return &SD_MMC;
  SD_MMC.end();
  ioSet(EXP_SD_POWER, true);
  return nullptr;
}

void sdUnmount() {
  SD_MMC.end();
  ioSet(EXP_SD_POWER, true);
}

}  // namespace board

#endif
