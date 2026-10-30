#if defined(BOARD_MESH_DECK)

#include "hal/mesh_deck/io_mesh_deck.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/mesh_deck/pins_mesh_deck.h"
#include "input/keymap_mesh_deck.h"

// Register map and bring-up from platy-aprs's mesh_deck_io.cpp (itself
// camillia-mt's aw9523.cpp and keyboard.cpp).
namespace mesh_deck {
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
  Serial.printf("[meshdeck] FT6636 %s\n", Wire.endTransmission() == 0 ? "ready" : "not answering");
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

}  // namespace

bool radioSleep() {
  // Chip select high first: a falling edge would wake it.
  digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_SCK, LOW);
  pinMode(LORA_SCK, OUTPUT);
  digitalWrite(LORA_MOSI, LOW);
  pinMode(LORA_MOSI, OUTPUT);
  pinMode(LORA_MISO, INPUT_PULLUP);
  pinMode(LORA_DIO1, INPUT_PULLDOWN);
  // Pulled up, BUSY stays high with no module; a radio drops it once reset.
  pinMode(LORA_BUSY, INPUT_PULLUP);
  digitalWrite(LORA_RST, LOW);
  pinMode(LORA_RST, OUTPUT);
  delay(2);
  digitalWrite(LORA_RST, HIGH);
  const uint32_t t0 = millis();
  while (digitalRead(LORA_BUSY) == HIGH) {
    if (millis() - t0 > 50) return false;
    delay(1);
  }
  // SetSleep (0x84), cold start: SPI mode 0, MSB first, bit-banged.
  const uint8_t cmd[] = {0x84, 0x00};
  digitalWrite(LORA_CS, LOW);
  delayMicroseconds(1);
  for (uint8_t b : cmd) {
    for (int i = 7; i >= 0; --i) {
      digitalWrite(LORA_MOSI, (b >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, HIGH);
      delayMicroseconds(1);
      digitalWrite(LORA_SCK, LOW);
    }
  }
  delayMicroseconds(1);
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(LORA_MOSI, LOW);
  return true;
}

bool ioBegin() {
  busRecover();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  expander = awBegin(EXPANDER_ADDR);
  if (!expander) {
    Serial.printf("[meshdeck] expander 0x%02X not found\n", EXPANDER_ADDR);
    return false;
  }
  releaseTouchReset();
  uint16_t v;
  Serial.printf("[meshdeck] MAX17048 %s\n", gaugeReg16(0x02, v) ? "ready" : "not answering");
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
      Serial.printf("[meshdeck] keyboard %s half found\n", h ? "right" : "left");
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
      Serial.printf("[meshdeck] keyboard %s half lost\n", h ? "right" : "left");
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

}  // namespace mesh_deck

#endif
