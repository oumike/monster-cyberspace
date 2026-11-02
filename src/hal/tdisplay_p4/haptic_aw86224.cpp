// SPDX-License-Identifier: GPL-3.0-only
// Ported from wadamesh variants/tdisplay_p4/P4Haptic.cpp: its AW86224
// register sequence is adapted from LilyGo's cpp_bus_driver Aw862xx driver
// (GPL-3.0, github.com/Llgok/cpp_bus_driver, src/chip/i2c/aw862xx.cpp),
// trimmed to RAM-mode playback of one waveform. Register names follow that
// driver and the AW86224 datasheet.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/haptic_aw86224.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <algorithm>
#include <cstring>

#include "hal/tdisplay_p4/haptic_aw86224_waveform.h"
#include "hal/tdisplay_p4/i2c1_tdisplay_p4.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::haptic {
namespace {

constexpr uint8_t kRegId = 0x00;        // chip id
constexpr uint8_t kRegPlayCfg2 = 0x07;  // RAM/RTP playback gain
constexpr uint8_t kRegPlayCfg3 = 0x08;  // [1:0] play mode (0 = RAM), [2] auto brake
constexpr uint8_t kRegPlayCfg4 = 0x09;  // [0] go, [1] stop
constexpr uint8_t kRegWavCfg1 = 0x0A;   // sequencer slot 1: waveform number
constexpr uint8_t kRegWavCfg2 = 0x0B;   // sequencer slot 2 (0 = end of sequence)
constexpr uint8_t kRegWavCfg9 = 0x12;   // [7:4] slot 1 loop count (15 = forever)
constexpr uint8_t kRegRtpCfg1 = 0x2D;   // [3:0] RAM base address high
constexpr uint8_t kRegRtpCfg2 = 0x2E;   // RAM base address low
constexpr uint8_t kRegRtpCfg3 = 0x2F;   // FIFO almost-empty/full thresholds (3 bytes)
constexpr uint8_t kRegGlbRd5 = 0x3F;    // [3:0] global state (0 = standby)
constexpr uint8_t kRegRamAddrH = 0x40;  // SRAM pointer (2 bytes)
constexpr uint8_t kRegRamData = 0x42;   // SRAM data port
constexpr uint8_t kRegSysCtrl1 = 0x43;  // [3] RAM init (SRAM access clock)
constexpr uint8_t kRegSysCtrl2 = 0x44;  // [1:0] sample rate (2 = 12 kHz), [6] force standby
constexpr uint8_t kRegSysCtrl7 = 0x49;  // [6] playing-changed gain bypass

constexpr uint16_t kBase = 0x0800;  // from the waveform header
constexpr uint8_t kWave = 1;        // an ~18 ms buzz, looped for the time asked
constexpr size_t kChunk = 32;       // SRAM bytes per transfer
constexpr uint8_t kGain = 0x80;     // LilyGo's default
constexpr uint32_t kBusWaitMs = 50;
constexpr int kMaxMs = 2000;

TwoWire* bus = nullptr;  // Wire1 while a burst holds it

bool write(uint8_t reg, uint8_t v) {
  bus->beginTransmission(HAPTIC_ADDR);
  bus->write(reg);
  bus->write(v);
  return bus->endTransmission() == 0;
}

bool writeN(uint8_t reg, const uint8_t* data, size_t len) {
  bus->beginTransmission(HAPTIC_ADDR);
  bus->write(reg);
  bus->write(data, len);
  return bus->endTransmission() == 0;
}

bool readN(uint8_t reg, uint8_t* data, size_t len) {
  bus->beginTransmission(HAPTIC_ADDR);
  bus->write(reg);
  if (bus->endTransmission(false) != 0) return false;
  if (bus->requestFrom(HAPTIC_ADDR, uint8_t(len)) != len) return false;
  for (size_t i = 0; i < len; ++i) data[i] = bus->read();
  return true;
}

bool read(uint8_t reg, uint8_t& v) { return readN(reg, &v, 1); }

// Clears mask, then sets bits.
bool update(uint8_t reg, uint8_t mask, uint8_t bits) {
  uint8_t v;
  return read(reg, v) && write(reg, uint8_t((v & ~mask) | bits));
}

bool setRamAddr(uint16_t addr) {
  const uint8_t a[2] = {uint8_t(addr >> 8), uint8_t(addr & 0xFF)};
  return writeN(kRegRamAddrH, a, sizeof a);
}

bool take() {
  bus = i2c1::acquire(i2c1::Audio, kBusWaitMs);
  return bus != nullptr;
}

void give() {
  bus = nullptr;
  i2c1::release();
}

enum class Stop { Done, Failed, Busy };

// The stop flag and forced standby, until the chip reports standby (about
// 100 ms at most). Called with the bus free: it is taken per read and given
// back between polls, so the codec's mute and the keyboard are not held up.
Stop stop() {
  if (!take()) return Stop::Busy;
  const bool asked = update(kRegPlayCfg4, 0x02, 0x02) && update(kRegSysCtrl2, 0x40, 0x40);
  give();
  if (!asked) return Stop::Failed;
  bool idle = false;
  const TickType_t start = xTaskGetTickCount();
  while (!idle && xTaskGetTickCount() - start < pdMS_TO_TICKS(100)) {
    if (take()) {
      uint8_t st;
      idle = read(kRegGlbRd5, st) && (st & 0x0F) == 0;
      give();
    }
    if (!idle) vTaskDelay(pdMS_TO_TICKS(1));
  }
  // Forced standby has to come off, so a busy bus gets a few more turns.
  bool cleared = false;
  for (int i = 0; i < 5 && !cleared; ++i) {
    if (!take()) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    cleared = update(kRegSysCtrl2, 0x40, 0x00);
    give();
  }
  return idle && cleared ? Stop::Done : Stop::Failed;
}

// The chip keeps its SRAM across a P4 reset while it stays powered, so it is
// compared first and loaded only when it differs. The pointer is set per
// chunk: it is not sure to survive between transfers.
bool sramMatches() {
  uint8_t buf[kChunk];
  for (size_t off = 0; off < sizeof kHapticWaveform; off += kChunk) {
    const size_t n = std::min(kChunk, sizeof kHapticWaveform - off);
    if (!setRamAddr(kBase + off) || !readN(kRegRamData, buf, n)) return false;
    if (memcmp(buf, kHapticWaveform + off, n) != 0) return false;
  }
  return true;
}

bool sramLoad() {
  for (size_t off = 0; off < sizeof kHapticWaveform; off += kChunk) {
    const size_t n = std::min(kChunk, sizeof kHapticWaveform - off);
    if (!setRamAddr(kBase + off) || !writeN(kRegRamData, kHapticWaveform + off, n)) return false;
  }
  return true;
}

// With the chip stopped and the bus held: (with SRAM access on) the base
// address, FIFO thresholds and the waveform.
bool load(bool& reloaded) {
  reloaded = false;
  if (!update(kRegSysCtrl1, 0x08, 0x08)) return false;  // RAM init on
  delayMicroseconds(500);
  const uint16_t ae = kBase >> 1, af = kBase - (kBase >> 2);  // the vendor's formula
  const uint8_t fifo[3] = {uint8_t(((ae >> 8) << 4) | ((af >> 8) & 0x0F)), uint8_t(ae & 0xFF),
                           uint8_t(af & 0xFF)};
  bool ok = update(kRegRtpCfg1, 0x0F, uint8_t((kBase >> 8) & 0x0F)) &&
            write(kRegRtpCfg2, uint8_t(kBase & 0xFF)) && writeN(kRegRtpCfg3, fifo, sizeof fifo);
  if (ok && !sramMatches()) {
    reloaded = true;
    ok = sramLoad() && sramMatches();
  }
  const bool ramOff = update(kRegSysCtrl1, 0x08, 0x00);  // RAM init off
  return ok && ramOff;
}

QueueHandle_t queue = nullptr;
bool ready = false;
uint8_t tries = 0;

// A few attempts, then give up rather than retrying on every buzz. A busy
// bus is not an attempt.
bool ensureReady() {
  if (ready) return true;
  if (tries >= 3) return false;
  if (!take()) return false;
  ++tries;
  uint8_t id = 0;
  bool reloaded = false;
  const bool present = read(kRegId, id);
  const bool rate = present && update(kRegSysCtrl2, 0x03, 0x02);  // 12 kHz waveform data
  give();
  if (rate && stop() == Stop::Done && take()) {
    ready = load(reloaded);
    give();
  }
  if (ready)
    Serial.printf("[haptic] AW86224 up (id 0x%02X, waveform %s)\n", id,
                  reloaded ? "loaded" : "kept");
  else
    Serial.println(present ? "[haptic] AW86224 init failed" : "[haptic] no AW86224");
  return ready;
}

// Slot 1 is the buzz looped forever and slot 2 the end, in RAM mode with auto
// brake.
void play(int ms) {
  if (!take()) return;
  const bool started = write(kRegWavCfg1, kWave) && write(kRegWavCfg2, 0) &&
                       update(kRegWavCfg9, 0xF0, 0xF0) && update(kRegPlayCfg3, 0x07, 0x04) &&
                       update(kRegSysCtrl7, 0x40, 0x40) && write(kRegPlayCfg2, kGain) &&
                       update(kRegPlayCfg4, 0x01, 0x01);
  give();
  if (!started) return;
  vTaskDelay(pdMS_TO_TICKS(ms));
  // The loop count is forever, so the stop is never dropped: it waits its
  // turn on the bus, 50 ms at a time (and a tick between tries, so a bus
  // that fails at once cannot spin this core).
  for (int fails = 0;;) {
    const Stop r = stop();
    if (r == Stop::Done) break;
    if (r == Stop::Busy) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    if (++fails >= 3) {
      Serial.println("[haptic] stop failed");
      break;
    }
  }
}

void task(void*) {
  int ms;
  for (;;) {
    if (xQueueReceive(queue, &ms, portMAX_DELAY) != pdTRUE) continue;
    if (ensureReady()) play(ms);
  }
}

}  // namespace

void begin() {
  if (queue) return;
  queue = xQueueCreate(1, sizeof(int));
  xTaskCreatePinnedToCore(task, "haptic", 4096, nullptr, 1, nullptr, 0);
}

void buzz(int ms) {
  if (!queue || ms <= 0) return;
  ms = std::min(ms, kMaxMs);
  xQueueOverwrite(queue, &ms);
}

}  // namespace tdp4::haptic

#endif
