#if defined(BOARD_CYBER_DECK_PLUS)

#include "hal/cyber_deck_plus/io_cyber_deck_plus.h"

#include <Arduino.h>
#include <driver/i2s_std.h>

#include "hal/attaky/attaky_core.h"
#include "hal/cyber_deck_plus/pins_cyber_deck_plus.h"
#include "ui/volume.h"

// The Core's own devices (AW9523 expanders, FT6636 touch, MAX17048 gauge, the
// keyboard module) are hal/attaky/attaky_core.cpp, shared with the Mesh Deck.
// This adds the gamepad's two TCA9555 halves and Media I/O's NS4168
// amplifier; the deck has no GPS UART and no SX1262 (IO17/18/21 are the
// amplifier's I2S lines here).
namespace cyber_deck_plus {
namespace {

// ── Gamepad: two TCA9555s, all pins inputs with the chip's own pull-ups, so a
// pressed button reads 0 and the byte comes back inverted.
constexpr uint8_t kTcaInput0 = 0x00, kTcaOutput0 = 0x02, kTcaConfig0 = 0x06;
constexpr uint8_t kPadAddr[2] = {PAD_LEFT_ADDR, PAD_RIGHT_ADDR};
bool padReady[2] = {};

bool padConfigure(uint8_t addr) {
  return attaky_core::writeReg(addr, kTcaOutput0, 0xFF) && attaky_core::writeReg(addr, kTcaConfig0, 0xFF);
}

// ── Media I/O: NS4168 over I2S. The amplifier has no enable line, so its
// inputs must never float: from audioBegin on, the I2S peripheral drives all
// three lines. A tone streams with silence around it; after kIdleMs with
// nothing to play the channel is disabled (the clocks stop, the pins stay
// driven by the peripheral) and the task sleeps until the next tone, so an
// idle deck isn't clocking the amplifier and waking the CPU every 16 ms.
constexpr int kFrames = 256;           // 16 ms at SPK_HZ
constexpr uint32_t kIdleMs = 1000;     // silence streamed before the channel stops
constexpr float kPi = 3.14159265f;
constexpr float kToneAmp = 6000.0f;    // ~18 % of full scale
constexpr float kRampFrames = 160.0f;  // 10 ms fade in/out, keeps taps click-free

i2s_chan_handle_t gTx = nullptr;
TaskHandle_t gTask = nullptr;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
volatile int gPendingFreq = 0, gPendingMs = 0;
volatile float gVolume = 1.0f;  // audioVolume

void audioTask(void*) {
  static int16_t buf[kFrames * 2];
  constexpr uint32_t kIdleFrames = kIdleMs * SPK_HZ / 1000;
  int freq = 0;
  int64_t left = 0, total = 0;
  uint32_t silent = 0;  // frames of silence streamed since the last tone
  bool enabled = true;  // audioBegin leaves the channel running
  float phase = 0;
  for (;;) {
    if (!enabled) {
      // Asleep until audioTone gives the notification.
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      if (i2s_channel_enable(gTx) != ESP_OK) continue;
      enabled = true;
      silent = 0;
    }
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
        s = int16_t(sinf(phase) * kToneAmp * gVolume * gain);
        --left;
        silent = 0;
      } else {
        ++silent;
      }
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
    }
    size_t written = 0;
    if (i2s_channel_write(gTx, buf, sizeof(buf), &written, portMAX_DELAY) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (silent >= kIdleFrames) {
      // A tone queued since the copy above would be lost; play it instead.
      portENTER_CRITICAL(&gMux);
      const bool queued = gPendingMs > 0;
      portEXIT_CRITICAL(&gMux);
      if (!queued && i2s_channel_disable(gTx) == ESP_OK) enabled = false;
      phase = 0;
    }
  }
}

// Undoes a half-made channel, so audioReady() reports only a working one.
void audioFail() {
  if (gTx) {
    i2s_channel_disable(gTx);  // harmless when it was never enabled
    i2s_del_channel(gTx);
    gTx = nullptr;
  }
  // Without a stream, hold the amplifier's three lines at a defined level:
  // driven low, never floating, never toggling.
  pinMode(SPK_BCLK, OUTPUT);
  digitalWrite(SPK_BCLK, LOW);
  pinMode(SPK_SDATA, OUTPUT);
  digitalWrite(SPK_SDATA, LOW);
  pinMode(SPK_LRCLK, OUTPUT);
  digitalWrite(SPK_LRCLK, LOW);
}

}  // namespace

void parkMic() {
  pinMode(MIC_SCK, OUTPUT);
  digitalWrite(MIC_SCK, LOW);
  pinMode(MIC_WS, OUTPUT);
  digitalWrite(MIC_WS, LOW);
  pinMode(MIC_SD, INPUT_PULLDOWN);
  pinMode(MIC_LR, INPUT_PULLDOWN);
}

bool gamepadProbe() {
  for (uint8_t h = 0; h < 2; ++h) {
    if (padReady[h]) continue;
    uint8_t v;
    padReady[h] = attaky_core::readRegs(kPadAddr[h], kTcaInput0, &v, 1) && padConfigure(kPadAddr[h]);
    if (padReady[h]) Serial.printf("[cyberdeck] gamepad %s half found\n", h ? "right" : "left");
  }
  return padReady[0] || padReady[1];
}

bool gamepadPresent() { return padReady[0] || padReady[1]; }

bool readGamepad(uint8_t& left, uint8_t& right) {
  left = right = 0;
  uint8_t* const out[2] = {&left, &right};
  bool any = false;
  for (uint8_t h = 0; h < 2; ++h) {
    if (!padReady[h]) continue;
    uint8_t v;
    // 0x00 is every button at once: a bad read, not a chord. Inverted after.
    if (attaky_core::readRegs(kPadAddr[h], kTcaInput0, &v, 1) && v != 0x00) {
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
    gTx = nullptr;
    audioFail();
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
    audioFail();
    return false;
  }
  // Core 0, like the other boards' audio tasks: the UI task runs on core 1.
  if (xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, &gTask, 0) != pdPASS) {
    gTask = nullptr;
    audioFail();
    return false;
  }
  return true;
}

bool audioReady() { return gTx != nullptr && gTask != nullptr; }

void audioTone(int freqHz, int ms) {
  if (!audioReady() || ms <= 0) return;
  portENTER_CRITICAL(&gMux);
  gPendingFreq = freqHz;
  gPendingMs = ms;
  portEXIT_CRITICAL(&gMux);
  xTaskNotifyGive(gTask);  // wakes the task if the channel is idle
}

void audioVolume(uint8_t pct) { gVolume = rc::volumeGain(pct); }

}  // namespace cyber_deck_plus

#endif
