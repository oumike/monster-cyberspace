// SPDX-License-Identifier: GPL-3.0-or-later
// The codec set up as nicolaielectronics es8156's es8156_configure() does it
// (the badge-bsp audio init), on Arduino 3's ESP_I2S.
#if defined(BOARD_TANMATSU)

#include "hal/tanmatsu/audio_es8156.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <math.h>

#include "hal/tanmatsu/coproc_tanmatsu.h"
#include "hal/tanmatsu/pins_tanmatsu.h"
#include "ui/volume.h"

namespace tanmatsu::audio {
namespace {

constexpr uint32_t kRate = 44100;
constexpr int kChunk = 128;  // frames per write
constexpr float kAmplitude = 7800.0f;
constexpr uint32_t kAmpSettleMs = 20;

constexpr uint8_t kRegPage = 0xFC, kRegVolume = 0x14;
// The badge-bsp's level at 100 % (180 * pct / 100); loudness is set in the
// samples instead, as on the other boards.
constexpr uint8_t kVolume = 180;

// es8156_configure(), register by register (page 0).
constexpr uint8_t kInit[][2] = {
    {0x02, 0x04}, {0x20, 0x2A}, {0x21, 0x3C}, {0x22, 0x00}, {0x24, 0x07}, {0x23, 0x04},
    {0x0A, 0x01}, {0x0B, 0x01}, {0x11, 0x00}, {0x0D, 0x14}, {0x18, 0x00}, {0x08, 0x3F},
    {0x00, 0x02}, {0x00, 0x03}, {0x25, 0x20}, {0x25, 0x20},
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
  coproc::lock();
  bool ok = writeCodec(kRegPage, 0);
  for (const auto& r : kInit) ok = ok && writeCodec(r[0], r[1]);
  ok = ok && writeCodec(kRegVolume, kVolume);
  coproc::unlock();
  return ok;
}

// Off between sounds: an idle amplifier hisses and clicks.
void amp(bool on) { coproc::write8(coproc::OUTPUTS, on ? 0x01 : 0x00); }

// A few attempts, then give up rather than retrying on every alert.
bool ensureReady() {
  if (ready) return true;
  if (tries >= 3) return false;
  ++tries;
  if (!codecBegin()) {
    Serial.println("[audio] ES8156 init failed");
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

}  // namespace tanmatsu::audio

#endif
