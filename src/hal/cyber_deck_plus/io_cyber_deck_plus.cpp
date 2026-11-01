#if defined(BOARD_CYBER_DECK_PLUS)

#include "hal/cyber_deck_plus/io_cyber_deck_plus.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <driver/i2s_std.h>

#include "hal/cyber_deck_plus/pins_cyber_deck_plus.h"
#include "input/keymap_mesh_deck.h"  // rc::meshDeckKeyPos (the keyboard's layout)

// Register-level bring-up follows the Mesh Deck HAL: the Cyber Deck Plus
// carries the same Core (AW9523 expanders, FT6636 touch, MAX17048 gauge) and
// the same keyboard module. What it adds is the gamepad's two TCA9555 halves
// and Media I/O's NS4168 amplifier and microSD slot, and what it drops is the
// GPS UART and the SX1262 (IO17/18/21 are the amplifier's I2S lines here).
namespace cyber_deck_plus {
namespace {

constexpr uint8_t kAwInput0 = 0x00, kAwOutput0 = 0x02, kAwOutput1 = 0x03, kAwConfig0 = 0x04,
                  kAwConfig1 = 0x05, kAwIntMask0 = 0x06, kAwIntMask1 = 0x07, kAwChipId = 0x10,
                  kAwLedMode0 = 0x12, kAwLedMode1 = 0x13, kAwSoftRst = 0x7F;
constexpr uint8_t kTouchRstBit = 1 << 3;  // front expander P13
constexpr uint8_t kFrontLedMask = 0x07;   // front expander P10-P12, active low

bool expander = false;

bool writeReg(uint8_t addr, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t* out, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(addr), int(n)) != n) return false;
  for (uint8_t i = 0; i < n; ++i) out[i] = uint8_t(Wire.read());
  return true;
}

// A reset can land mid-transfer and leave a device holding SDA low; nine
// clocks and a STOP free it. Harmless on an idle bus.
void busRecover() {
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SCL, HIGH);
  for (int i = 0; i < 9; ++i) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
  }
  pinMode(I2C_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(I2C_SDA, HIGH);
  delayMicroseconds(5);
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(10);
}

// An AW9523 keeps its state across a CPU reset, so soft-reset it once and put
// every pin in GPIO input mode. Only once on the front expander: a second
// reset would hold the touch controller in reset again.
bool awBegin(uint8_t addr) {
  uint8_t id = 0;
  bool ok = false;
  for (int i = 0; i < 3 && !ok; ++i) {
    if (i) delay(5);
    ok = readRegs(addr, kAwChipId, &id, 1) && id == 0x23;
  }
  if (!ok) return false;
  writeReg(addr, kAwSoftRst, 0x00);
  delay(2);
  return writeReg(addr, kAwLedMode0, 0xFF) && writeReg(addr, kAwLedMode1, 0xFF) &&
         writeReg(addr, kAwConfig0, 0xFF) && writeReg(addr, kAwConfig1, 0xFF) &&
         writeReg(addr, kAwIntMask0, 0xFF) && writeReg(addr, kAwIntMask1, 0xFF);
}

// The FT6636's reset (P13) is low at power-up: pulse it and let it boot.
void releaseTouchReset() {
  uint8_t cfg = 0xFF, out = 0xFF;
  readRegs(EXPANDER_ADDR, kAwConfig1, &cfg, 1);
  readRegs(EXPANDER_ADDR, kAwOutput1, &out, 1);
  // The LED pins go high (off) before they become outputs.
  out |= kFrontLedMask;
  writeReg(EXPANDER_ADDR, kAwOutput1, uint8_t(out & ~kTouchRstBit));
  writeReg(EXPANDER_ADDR, kAwConfig1, uint8_t(cfg & ~(kTouchRstBit | kFrontLedMask)));
  delay(10);
  writeReg(EXPANDER_ADDR, kAwOutput1, uint8_t(out | kTouchRstBit));
  delay(300);
  Wire.beginTransmission(TOUCH_ADDR);
  Serial.printf("[cyberdeck] FT6636 %s\n", Wire.endTransmission() == 0 ? "ready" : "not answering");
}

bool gaugeReg16(uint8_t reg, uint16_t& v) {
  uint8_t b[2];
  if (!readRegs(GAUGE_ADDR, reg, b, 2)) return false;
  v = uint16_t((b[0] << 8) | b[1]);
  return true;
}

// ── Keyboard module: per half, rows P10-P14 driven low one at a time,
// columns P00-P04 read back; P15-P17 the half's RGB LED (active low).
constexpr uint8_t kKbAddr[2] = {KB_LEFT_ADDR, KB_RIGHT_ADDR};
constexpr uint8_t kRows = 0x1F, kKbLedOff = 0xE0;
constexpr uint8_t kReleaseScans = 2, kLostScans = 10;
uint8_t kbLed = kKbLedOff;  // rides along in every row write
bool kbReady[2] = {};
uint8_t kbFails[2] = {};
uint8_t kbStable[2][5] = {}, kbReported[2][5] = {}, kbRelCand[2][5] = {}, kbRelCount[2][5] = {};

bool kbConfigure(uint8_t h) {
  const uint8_t a = kKbAddr[h];
  return writeReg(a, kAwConfig0, 0xFF) && writeReg(a, kAwConfig1, 0x00) &&
         writeReg(a, kAwOutput0, 0x00) && writeReg(a, kAwOutput1, kRows | kbLed);
}

bool kbScanHalf(uint8_t h, uint8_t rows[5]) {
  const uint8_t a = kKbAddr[h];
  for (uint8_t r = 0; r < 5; ++r) {
    if (!writeReg(a, kAwOutput1, uint8_t((kRows & ~(1u << r)) | kbLed))) return false;
    delayMicroseconds(50);
    uint8_t cols;
    if (!readRegs(a, kAwInput0, &cols, 1)) return false;
    rows[r] = uint8_t(~cols & 0x1F);
    if (rows[r] == 0x1F) return false;  // a glitched read, not five fingers
  }
  writeReg(a, kAwOutput1, kRows | kbLed);
  return true;
}

// A press counts on the first scan that sees it; a release must hold for
// kReleaseScans scans.
void kbDebounce(uint8_t h, const uint8_t fresh[5]) {
  for (uint8_t r = 0; r < 5; ++r) {
    uint8_t& stable = kbStable[h][r];
    stable |= fresh[r];
    const uint8_t rel = uint8_t(stable & ~fresh[r]);
    if (!rel) {
      kbRelCand[h][r] = kbRelCount[h][r] = 0;
    } else if (rel != kbRelCand[h][r]) {
      kbRelCand[h][r] = rel;
      kbRelCount[h][r] = 1;
    } else if (++kbRelCount[h][r] >= kReleaseScans) {
      stable &= uint8_t(~rel);
      kbRelCand[h][r] = kbRelCount[h][r] = 0;
    }
  }
}

// ── Gamepad: two TCA9555s, all pins inputs with the chip's own pull-ups, so a
// pressed button reads 0 and the byte comes back inverted.
constexpr uint8_t kTcaInput0 = 0x00, kTcaOutput0 = 0x02, kTcaConfig0 = 0x06;
bool padReady[2] = {};

bool padConfigure(uint8_t addr) {
  return writeReg(addr, kTcaOutput0, 0xFF) && writeReg(addr, kTcaConfig0, 0xFF);
}

// ── Media I/O: NS4168 over I2S. The task writes forever — silence when no tone
// is queued — because the amplifier has no enable line, so a stopped clock or
// floating data is audible and a running zero stream is not.
constexpr int kFrames = 256;  // 16 ms at SPK_HZ
constexpr float kPi = 3.14159265f;
constexpr float kToneAmp = 6000.0f;  // ~18 % of full scale
constexpr float kRampFrames = 160.0f;  // 10 ms fade in/out, keeps taps click-free

i2s_chan_handle_t gTx = nullptr;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
volatile int gPendingFreq = 0, gPendingMs = 0;

void audioTask(void*) {
  static int16_t buf[kFrames * 2];
  int freq = 0;
  int64_t left = 0, total = 0;
  float phase = 0;
  for (;;) {
    portENTER_CRITICAL(&gMux);
    if (gPendingMs > 0) {
      freq = gPendingFreq;
      total = int64_t(gPendingMs) * int64_t(SPK_HZ) / 1000;
      left = total;
      gPendingMs = 0;
    }
    portEXIT_CRITICAL(&gMux);
    for (int i = 0; i < kFrames; ++i) {
      int16_t s = 0;
      if (left > 0) {
        phase += 2.0f * kPi * float(freq) / float(SPK_HZ);
        if (phase >= 2.0f * kPi) phase -= 2.0f * kPi;
        float gain = 1.0f;
        const float down = float(left) / kRampFrames;
        if (down < gain) gain = down;
        const float up = float(total - left) / kRampFrames;
        if (up < gain) gain = up;
        s = int16_t(sinf(phase) * kToneAmp * gain);
        --left;
      }
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
    }
    size_t written = 0;
    if (i2s_channel_write(gTx, buf, sizeof(buf), &written, portMAX_DELAY) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// Without a stream, hold the amplifier's three lines at a defined level: driven
// low, never floating, never toggling.
void parkAmp() {
  pinMode(SPK_BCLK, OUTPUT);
  digitalWrite(SPK_BCLK, LOW);
  pinMode(SPK_SDATA, OUTPUT);
  digitalWrite(SPK_SDATA, LOW);
  pinMode(SPK_LRCLK, OUTPUT);
  digitalWrite(SPK_LRCLK, LOW);
}

// The microphone is an I2S slave: its clock and word-select come from us, so
// hold them low until something actually records.
void parkMic() {
  pinMode(MIC_SCK, OUTPUT);
  digitalWrite(MIC_SCK, LOW);
  pinMode(MIC_WS, OUTPUT);
  digitalWrite(MIC_WS, LOW);
  pinMode(MIC_SD, INPUT_PULLDOWN);
  pinMode(MIC_LR, INPUT_PULLDOWN);
}

}  // namespace

bool ioBegin() {
  busRecover();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  parkMic();
  expander = awBegin(EXPANDER_ADDR);
  if (!expander) {
    Serial.printf("[cyberdeck] expander 0x%02X not found\n", EXPANDER_ADDR);
    return false;
  }
  releaseTouchReset();
  uint16_t v;
  Serial.printf("[cyberdeck] MAX17048 %s\n", gaugeReg16(0x02, v) ? "ready" : "not answering");
  return true;
}

bool readButtons(uint8_t& pressed) {
  pressed = 0;
  if (expander) {
    uint8_t v;
    // All zero is every line low at once: a bad read, not a chord.
    if (!readRegs(EXPANDER_ADDR, kAwInput0, &v, 1) || v == 0x00) return false;
    pressed = uint8_t(~v & ~BTN_POWER_BIT);
  }
  if (digitalRead(BOOT_BTN) == LOW) pressed |= rc::kPadBoot;
  return true;
}

bool readTouch(int16_t& x, int16_t& y) {
  uint8_t b[7];
  if (!readRegs(TOUCH_ADDR, 0x02, b, 7)) return false;  // TD_STATUS and the first point
  const uint8_t points = b[0] & 0x0F;
  if (points == 0 || points > 2) return false;
  x = int16_t(((b[1] & 0x0F) << 8) | b[2]);
  y = int16_t(((b[3] & 0x0F) << 8) | b[4]);
  return true;
}

int batteryPercent() {
  uint16_t raw;
  if (!gaugeReg16(0x04, raw)) return -1;  // SOC, high byte whole percent
  return min(int(raw >> 8), 100);
}

uint16_t batteryMv() {
  uint16_t raw;
  if (!gaugeReg16(0x02, raw)) return 0;  // VCELL, 78.125 uV/LSB
  return uint16_t(uint32_t(raw) * 78125UL / 1000000UL);
}

bool batteryRate(int16_t& rate) {
  uint16_t raw;
  if (!gaugeReg16(0x16, raw)) return false;  // CRATE
  rate = int16_t(raw);
  return true;
}

void keyboardProbe() {
  for (uint8_t h = 0; h < 2; ++h) {
    if (kbReady[h]) continue;
    kbReady[h] = awBegin(kKbAddr[h]) && kbConfigure(h);
    if (kbReady[h]) {
      kbFails[h] = 0;
      Serial.printf("[cyberdeck] keyboard %s half found\n", h ? "right" : "left");
    }
  }
}

bool keyboardPresent() { return kbReady[0] || kbReady[1]; }

int keyboardScan(KeyChange* out, int max) {
  int n = 0;
  for (uint8_t h = 0; h < 2; ++h) {
    if (!kbReady[h]) continue;
    uint8_t fresh[5];
    if (kbScanHalf(h, fresh)) {
      kbFails[h] = 0;
      kbDebounce(h, fresh);
    } else if (++kbFails[h] >= kLostScans) {
      // Unplugged: release whatever it held, and probe for it again later.
      kbReady[h] = false;
      Serial.printf("[cyberdeck] keyboard %s half lost\n", h ? "right" : "left");
      for (uint8_t r = 0; r < 5; ++r) kbStable[h][r] = kbRelCand[h][r] = kbRelCount[h][r] = 0;
    }
    for (uint8_t r = 0; r < 5; ++r) {
      const uint8_t changed = uint8_t(kbStable[h][r] ^ kbReported[h][r]);
      for (uint8_t c = 0; c < 5 && changed; ++c) {
        const uint8_t m = uint8_t(1u << c);
        if (!(changed & m) || n >= max) continue;
        kbReported[h][r] ^= m;
        out[n++] = {rc::meshDeckKeyPos(h, r, c), bool(kbStable[h][r] & m)};
      }
    }
  }
  return n;
}

void setLed(uint8_t rgb) {
  if (expander) {
    uint8_t out;
    if (readRegs(EXPANDER_ADDR, kAwOutput1, &out, 1))
      writeReg(EXPANDER_ADDR, kAwOutput1, uint8_t((out | kFrontLedMask) & ~(rgb & 7)));
  }
  kbLed = uint8_t(kKbLedOff & ~(((rgb & 1) ? 0x80 : 0) | ((rgb & 2) ? 0x40 : 0) | ((rgb & 4) ? 0x20 : 0)));
  for (uint8_t h = 0; h < 2; ++h)
    if (kbReady[h]) writeReg(kKbAddr[h], kAwOutput1, kRows | kbLed);
}

bool gamepadProbe() {
  const uint8_t addr[2] = {PAD_LEFT_ADDR, PAD_RIGHT_ADDR};
  for (uint8_t h = 0; h < 2; ++h) {
    if (padReady[h]) continue;
    uint8_t v;
    padReady[h] = readRegs(addr[h], kTcaInput0, &v, 1) && padConfigure(addr[h]);
    if (padReady[h]) Serial.printf("[cyberdeck] gamepad %s half found\n", h ? "right" : "left");
  }
  return padReady[0] || padReady[1];
}

bool gamepadPresent() { return padReady[0] || padReady[1]; }

bool readGamepad(uint8_t& left, uint8_t& right) {
  left = right = 0;
  const uint8_t addr[2] = {PAD_LEFT_ADDR, PAD_RIGHT_ADDR};
  uint8_t* const out[2] = {&left, &right};
  bool any = false;
  for (uint8_t h = 0; h < 2; ++h) {
    if (!padReady[h]) continue;
    uint8_t v;
    // 0x00 is every button at once: a bad read, not a chord. Inverted after.
    if (readRegs(addr[h], kTcaInput0, &v, 1) && v != 0x00) {
      *out[h] = uint8_t(~v);
      any = true;
    }
  }
  return any;
}

bool audioBegin() {
  i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan.auto_clear = true;
  if (i2s_new_channel(&chan, &gTx, nullptr) != ESP_OK) {
    parkAmp();
    return false;
  }
  i2s_std_config_t cfg = {};
  cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SPK_HZ);
  cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  cfg.gpio_cfg.bclk = gpio_num_t(SPK_BCLK);
  cfg.gpio_cfg.ws = gpio_num_t(SPK_LRCLK);
  cfg.gpio_cfg.dout = gpio_num_t(SPK_SDATA);
  cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(gTx, &cfg) != ESP_OK || i2s_channel_enable(gTx) != ESP_OK) {
    parkAmp();
    return false;
  }
  // Core 1: the UI and the network workers keep core 0.
  if (xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 1) != pdPASS) {
    parkAmp();
    return false;
  }
  return true;
}

bool audioReady() { return gTx != nullptr; }

void audioTone(int freqHz, int ms) {
  if (!gTx || ms <= 0) return;
  portENTER_CRITICAL(&gMux);
  gPendingFreq = freqHz;
  gPendingMs = ms;
  portEXIT_CRITICAL(&gMux);
}

}  // namespace cyber_deck_plus

#endif
