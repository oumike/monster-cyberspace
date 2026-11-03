#if defined(BOARD_WIO_L2) || defined(BOARD_PAGER) || defined(BOARD_CARDPUTER_ADV) || \
    defined(BOARD_TDISPLAY_P4)

#include "hal/wio_l2/audio_es8311.h"

#include <Arduino.h>
#include <AudioBoard.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <math.h>

#include "ui/volume.h"


// Ported from platy-aprs's src/hal/audio.cpp (itself from camillia-mt's
// pager/T-Deck tone players), on Arduino 3's ESP_I2S instead of the legacy
// I2S driver. Shared by the Wio Tracker L2, the T-LoRa Pager and the
// Cardputer ADV, and the T-Display P4.

namespace es8311 {
namespace {

constexpr uint32_t kRate = 44100;
constexpr int kChunk = 128;  // frames per write
constexpr float kAmplitude = 7800.0f;
constexpr uint8_t kVolActive = 50, kVolIdle = 10;

Config cfg{};
volatile float gain = 1.0f;  // setVolume

void amp(bool on) {
  if (cfg.amp) cfg.amp(on);
}

// ES8311 clock manager registers, as M5Unified programs them for the
// Cardputer ADV (no MCLK line): 0x01 = 0xB5, MCLK from BCLK with the DAC
// (speaker) path clocked; 0x02 = 0x18, BCLK multiplied by 8 (16-bit stereo
// BCLK is 32 fs, so 256 fs).
constexpr uint8_t kRegClk1 = 0x01, kRegClk2 = 0x02;
constexpr uint8_t kClk1FromBclk = 0xB5, kClk2FromBclk = 0x18;

TwoWire& wire() { return cfg.wire ? *cfg.wire : Wire; }

bool writeCodec(uint8_t reg, uint8_t v) {
  wire().beginTransmission(cfg.addr);
  wire().write(reg);
  wire().write(v);
  return wire().endTransmission() == 0;
}

// Holds the bus; ok is false when tryLock gave up, and nothing may be sent.
struct Guard {
  bool ok = true;
  Guard() {
    if (cfg.tryLock)
      ok = cfg.tryLock();
    else if (cfg.lock)
      cfg.lock();
  }
  ~Guard() {
    if (ok && cfg.unlock) cfg.unlock();
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

// busy: the bus was not free, which does not count as a failed attempt.
bool codecBegin(bool& busy) {
  busy = false;
  static bool pinsSet = false;
  if (!pinsSet) {
    pins.addI2C(audio_driver::PinFunction::CODEC, wire());
    pins.addI2S(audio_driver::PinFunction::CODEC, cfg.mclkFromBclk ? -1 : cfg.mclk, cfg.bck,
                cfg.ws, cfg.dout, -1);
    (void)codec.driver().setI2CAddress(cfg.addr);
    if (cfg.mclkFromBclk)
      audio_driver::AudioDriverES8311.setMasterClockSource(audio_driver::ES8311::FROM_SCLK_PIN);
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
  if (!g.ok) {
    busy = true;
    return false;
  }
  if (!codec.begin(c)) return false;
  if (cfg.mclkFromBclk &&
      !(writeCodec(kRegClk1, kClk1FromBclk) && writeCodec(kRegClk2, kClk2FromBclk)))
    return false;
  codec.setVolume(kVolIdle);
  // Muted and unpowered between sounds: an idle DAC and amp click.
  codec.setMute(true);
  amp(false);
  return true;
}

bool i2sBegin() {
  i2s.setPins(cfg.bck, cfg.ws, cfg.dout, -1, cfg.mclkFromBclk ? -1 : cfg.mclk);
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
  // Codec, then I2S: the order platy-aprs runs on this board.
  bool busy;
  if (!codecBegin(busy)) {
    if (busy) return false;
    ++tries;
    Serial.println("[audio] ES8311 init failed");
    return false;
  }
  ++tries;
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
      int16_t v = (int16_t)(sinf(phase) * kAmplitude * gain * env);
      pcm[k * 2] = pcm[k * 2 + 1] = v;
      phase += step;
      if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
    }
    i2s.write(reinterpret_cast<const uint8_t*>(pcm), (size_t)n * 2 * sizeof(int16_t));
  }
}

// A transfer that could not get the bus drops the tone (tryLock only).
void play(const Job& job) {
  amp(true);
  delay(cfg.ampSettleMs);
  bool on;
  {
    Guard g;
    on = g.ok;
    if (on) codec.setVolume(kVolActive);  // raised while still muted: inaudible
  }
  if (on) {
    writeSilence(6);  // prime the DMA so the start isn't clipped
    Guard g;
    on = g.ok;
    if (on) codec.setMute(false);
  }
  if (on) {
    writeTone(job.hz, job.ms);
    writeSilence(20);
  }
  {
    Guard g;
    if (g.ok) {
      codec.setMute(true);
      codec.setVolume(kVolIdle);
    }
  }
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

void setVolume(uint8_t pct) { gain = rc::volumeGain(pct); }

}  // namespace es8311

#endif
