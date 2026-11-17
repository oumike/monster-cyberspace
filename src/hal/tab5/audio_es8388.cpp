// SPDX-License-Identifier: GPL-3.0-or-later
// The codec set up as M5Unified's _speaker_enabled_cb_tab5 does it, on
// Arduino 3's ESP_I2S (MCLK at 256 fs, as M5Unified's speaker).
#if defined(BOARD_TAB5)

#include "hal/tab5/audio_es8388.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <math.h>

#include "hal/tab5/io_tab5.h"
#include "hal/tab5/pins_tab5.h"
#include "ui/volume.h"

namespace tab5::audio {
namespace {

constexpr uint32_t kRate = 48000;
constexpr int kChunk = 128;  // frames per write
constexpr float kAmplitude = 7800.0f;
constexpr uint32_t kAmpSettleMs = 20;

// M5Unified's sequence, register by register: reset, power up the DAC and
// its four outputs, slave mode, 16-bit I2S, DAC unmuted at full volume, the
// mixers on the DAC, outputs at 0x21.
constexpr uint8_t kInit[][2] = {
    {0, 0x80}, {0, 0x00}, {0, 0x00}, {0, 0x0E}, {1, 0x00},  {2, 0x0A},  {3, 0xFF},
    {4, 0x3C}, {5, 0x00}, {6, 0x00}, {7, 0x7C}, {8, 0x00},  {23, 0x18}, {24, 0x00},
    {25, 0x20}, {26, 0x00}, {27, 0x00}, {28, 0x08}, {29, 0x00}, {38, 0x00}, {39, 0xB8},
    {42, 0xB8}, {43, 0x08}, {45, 0x00}, {46, 0x21}, {47, 0x21}, {48, 0x21}, {49, 0x21},
};

volatile float gain = 1.0f;

struct Job {
  uint16_t hz, ms;
};

QueueHandle_t queue = nullptr;
bool ready = false;
uint8_t tries = 0;
I2SClass i2s;

bool writeCodec(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(CODEC_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool codecBegin() {
  bool ok = true;
  for (const auto& r : kInit) ok = ok && writeCodec(r[0], r[1]);
  return ok;
}

// Off between sounds: an idle amplifier hisses and clicks.
void amp(bool on) { io::set(EXP1_ADDR, EXP1_SPK_AMP, on); }

// A few attempts, then give up rather than retrying on every alert.
bool ensureReady() {
  if (ready) return true;
  if (tries >= 3) return false;
  ++tries;
  if (!codecBegin()) {
    Serial.println("[audio] ES8388 init failed");
    return false;
  }
  i2s.setPins(I2S_BCK, I2S_WS, I2S_DOUT, -1, I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[audio] i2s setup failed");
    return false;
  }
  ready = true;
  Serial.println("[audio] ready");
  return true;
}

void writeSilence(uint32_t ms) {
  static const int16_t zero[kChunk * 2] = {};
  for (uint32_t left = ms * kRate / 1000; left;) {
    uint32_t n = left > (uint32_t)kChunk ? kChunk : left;
    i2s.write(reinterpret_cast<const uint8_t*>(zero), n * 2 * sizeof(int16_t));
    left -= n;
  }
}

// A sine with short attack and release ramps, so a beep starts and stops
// without a pop.
void writeTone(uint16_t hz, uint16_t ms) {
  const uint32_t total = (uint32_t)ms * kRate / 1000;
  if (!total) return;
  uint32_t ramp = kRate / 400;
  if (ramp > total / 5) ramp = total / 5;
  if (ramp < 8) ramp = 8;
  const float step = 2.0f * (float)M_PI * hz / kRate;
  float phase = 0;
  int16_t pcm[kChunk * 2];
  for (uint32_t i = 0; i < total;) {
    int n = total - i > (uint32_t)kChunk ? kChunk : (int)(total - i);
    for (int k = 0; k < n; ++k, ++i) {
      float env = 1.0f;
      if (i < ramp) env = (float)i / ramp;
      if (total - i < ramp) env = fminf(env, (float)(total - i) / ramp);
      int16_t v = (int16_t)(sinf(phase) * kAmplitude * gain * env);
      pcm[k * 2] = pcm[k * 2 + 1] = v;
      phase += step;
      if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
    }
    i2s.write(reinterpret_cast<const uint8_t*>(pcm), (size_t)n * 2 * sizeof(int16_t));
  }
}

void play(const Job& job) {
  writeSilence(6);  // prime the DMA so the start isn't clipped
  amp(true);
  delay(kAmpSettleMs);
  writeTone(job.hz, job.ms);
  writeSilence(20);
  amp(false);
}

void task(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(queue, &job, portMAX_DELAY) != pdTRUE) continue;
    if (ensureReady()) play(job);
  }
}

}  // namespace

void begin() {
  if (queue) return;
  queue = xQueueCreate(1, sizeof(Job));
  xTaskCreatePinnedToCore(task, "audio", 6144, nullptr, 1, nullptr, 0);
}

void beep(int freqHz, int ms) {
  if (!queue || freqHz <= 0 || ms <= 0) return;
  Job job{(uint16_t)freqHz, (uint16_t)(ms > 2000 ? 2000 : ms)};
  xQueueOverwrite(queue, &job);
}

void setVolume(uint8_t pct) { gain = rc::volumeGain(pct); }

}  // namespace tab5::audio

#endif
