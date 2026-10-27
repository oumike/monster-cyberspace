#if defined(BOARD_TDECK)

#include "hal/tdeck/audio_tdeck.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "hal/tdeck/pins_tdeck.h"

namespace tdeck_audio {
namespace {

constexpr uint32_t kRate = 16000;
constexpr int kChunk = 128;  // samples per write
constexpr int16_t kAmplitude = 3000;

struct Job {
  uint16_t hz, ms;
};

QueueHandle_t queue = nullptr;
I2SClass i2s;

bool interrupted() { return uxQueueMessagesWaiting(queue) > 0; }

void writeSilence(uint32_t ms) {
  static const int16_t zero[kChunk] = {};
  for (uint32_t left = ms * kRate / 1000; left;) {
    uint32_t n = left > (uint32_t)kChunk ? kChunk : left;
    i2s.write(reinterpret_cast<const uint8_t*>(zero), n * sizeof(int16_t));
    left -= n;
  }
}

// Stops early when a newer beep is queued.
void writeTone(uint16_t hz, uint16_t ms) {
  const uint32_t total = (uint32_t)ms * kRate / 1000;
  const uint32_t half = hz ? kRate / (2u * hz) : 0;
  if (!total || !half) return;
  int16_t pcm[kChunk];
  for (uint32_t i = 0; i < total && !interrupted();) {
    int n = total - i > (uint32_t)kChunk ? kChunk : (int)(total - i);
    for (int k = 0; k < n; ++k, ++i) pcm[k] = (i / half) & 1 ? -kAmplitude : kAmplitude;
    i2s.write(reinterpret_cast<const uint8_t*>(pcm), (size_t)n * sizeof(int16_t));
  }
}

void task(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(queue, &job, portMAX_DELAY) != pdTRUE) continue;
    i2s.setPins(tdeck::I2S_BCK, tdeck::I2S_WS, tdeck::I2S_DOUT);
    if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
      Serial.println("[audio] i2s setup failed");
      continue;
    }
    writeSilence(5);  // prime the DMA so the start isn't clipped
    writeTone(job.hz, job.ms);
    writeSilence(20);
    // Stopped between sounds, so an idle amp doesn't hiss.
    i2s.end();
  }
}

}  // namespace

void begin() {
  if (queue) return;
  queue = xQueueCreate(1, sizeof(Job));
  if (!queue) {
    Serial.println("[audio] queue failed");
    return;
  }
  if (xTaskCreatePinnedToCore(task, "audio", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("[audio] task failed");
    vQueueDelete(queue);
    queue = nullptr;
  }
}

void beep(int freqHz, int ms) {
  if (!queue || freqHz <= 0 || ms <= 0) return;
  Job job{(uint16_t)freqHz, (uint16_t)(ms > 2000 ? 2000 : ms)};
  xQueueOverwrite(queue, &job);
}

}  // namespace tdeck_audio

#endif
