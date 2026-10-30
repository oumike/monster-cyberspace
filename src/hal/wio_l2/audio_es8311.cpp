#if defined(BOARD_WIO_L2) || defined(BOARD_PAGER) || defined(BOARD_TDISPLAY_P4)

#include "hal/wio_l2/audio_es8311.h"

#include <Arduino.h>
#include <AudioBoard.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <math.h>


// Ported from platy-aprs's src/hal/audio.cpp (itself from camillia-mt's
// pager/T-Deck tone players), on Arduino 3's ESP_I2S instead of the legacy
// I2S driver. Shared by the Wio Tracker L2, the T-LoRa Pager and the T-Display P4.

namespace es8311 {
namespace {

constexpr uint32_t kRate = 44100;
constexpr int kChunk = 128;  // frames per write
constexpr float kAmplitude = 7800.0f;
constexpr uint8_t kVolActive = 50, kVolIdle = 10;

Config cfg{};

struct Guard {
  Guard() {
    if (cfg.lock) cfg.lock();
  }
  ~Guard() {
    if (cfg.unlock) cfg.unlock();
  }
};

struct Job {
  uint16_t hz, ms;
};

QueueHandle_t queue = nullptr;
bool ready = false;
uint8_t tries = 0;
I2SClass i2s;
audio_driver::DriverPins pins;
audio_driver::AudioBoard codec(audio_driver::AudioDriverES8311, pins);

bool codecBegin() {
  static bool pinsSet = false;
  if (!pinsSet) {
    pins.addI2C(audio_driver::PinFunction::CODEC, cfg.wire ? *cfg.wire : Wire);
    pins.addI2S(audio_driver::PinFunction::CODEC, cfg.mclk, cfg.bck, cfg.ws, cfg.dout, -1);
    (void)codec.driver().setI2CAddress(cfg.addr);
    pinsSet = true;
  }
  audio_driver::CodecConfig c;
  c.input_device = audio_driver::ADC_INPUT_NONE;
  c.output_device = audio_driver::DAC_OUTPUT_ALL;
  c.i2s.bits = audio_driver::BIT_LENGTH_16BITS;
  c.i2s.rate = audio_driver::RATE_44K;
  c.i2s.channels = audio_driver::CHANNELS2;
  c.i2s.fmt = audio_driver::I2S_NORMAL;
  c.i2s.mode = audio_driver::MODE_SLAVE;
  Guard g;
  if (!codec.begin(c)) return false;
  codec.setVolume(kVolIdle);
  // Muted and unpowered between sounds: an idle DAC and amp click.
  codec.setMute(true);
  cfg.amp(false);
  return true;
}

bool i2sBegin() {
  i2s.setPins(cfg.bck, cfg.ws, cfg.dout, -1, cfg.mclk);
  if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[audio] i2s setup failed");
    return false;
  }
  return true;
}

// A few attempts, then give up rather than retrying on every alert.
bool ensureReady() {
  if (ready) return true;
  if (tries >= 3) return false;
  ++tries;
  // Codec, then I2S: the order platy-aprs runs on this board.
  if (!codecBegin()) {
    Serial.println("[audio] ES8311 init failed");
    return false;
  }
  if (!i2sBegin()) return false;
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
      int16_t v = (int16_t)(sinf(phase) * kAmplitude * env);
      pcm[k * 2] = pcm[k * 2 + 1] = v;
      phase += step;
      if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
    }
    i2s.write(reinterpret_cast<const uint8_t*>(pcm), (size_t)n * 2 * sizeof(int16_t));
  }
}

void play(const Job& job) {
  cfg.amp(true);
  delay(cfg.ampSettleMs);
  {
    Guard g;
    codec.setVolume(kVolActive);  // raised while still muted: inaudible
  }
  writeSilence(6);  // prime the DMA so the start isn't clipped
  {
    Guard g;
    codec.setMute(false);
  }
  writeTone(job.hz, job.ms);
  writeSilence(20);
  {
    Guard g;
    codec.setMute(true);
    codec.setVolume(kVolIdle);
  }
  cfg.amp(false);
}

void task(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(queue, &job, portMAX_DELAY) != pdTRUE) continue;
    if (ensureReady()) play(job);
  }
}

}  // namespace

void begin(const Config& c) {
  if (queue) return;
  cfg = c;
  queue = xQueueCreate(1, sizeof(Job));
  xTaskCreatePinnedToCore(task, "audio", 6144, nullptr, 1, nullptr, 0);
}

void beep(int freqHz, int ms) {
  if (!queue || freqHz <= 0 || ms <= 0) return;
  Job job{(uint16_t)freqHz, (uint16_t)(ms > 2000 ? 2000 : ms)};
  xQueueOverwrite(queue, &job);
}

}  // namespace es8311

#endif
