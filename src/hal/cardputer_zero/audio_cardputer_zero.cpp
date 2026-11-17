// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's beeps: a short square wave, played on their own thread
// (spec §5). PipeWire owns the ES8389 and ALSA's "default" is HDMI on the
// device, so beeps go through PulseAudio (PipeWire's pulse server), loaded at
// run time as M5's own apps do; ALSA's default PCM is the fallback.
#if defined(BOARD_CARDPUTER_ZERO)

#include <alsa/asoundlib.h>
#include <dlfcn.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "cp0_internal.h"
#include "hal/board.h"

namespace cp0 {

namespace {

constexpr unsigned kRate = 48000;
constexpr int kMaxMs = 2000;
constexpr int16_t kFullScale = 12000;  // a square wave at full scale is harsh

std::mutex mu;
std::condition_variable cv;
std::thread worker;
bool running = false;
bool pending = false;
int nextHz = 0, nextMs = 0;

std::atomic<int> appPct{100};   // board::setVolume
std::atomic<int> keyPct{100};   // the volume keys
std::atomic<bool> muted{false};

// libpulse-simple's stable C ABI, declared here so the build needs no
// PulseAudio headers and the package no PulseAudio dependency.
struct PaSampleSpec {
  int format;
  uint32_t rate;
  uint8_t channels;
};
constexpr int kPaSampleS16Le = 3;
constexpr int kPaStreamPlayback = 1;
using PaNew = void* (*)(const char*, const char*, int, const char*, const char*,
                        const PaSampleSpec*, const void*, const void*, int*);
using PaWrite = int (*)(void*, const void*, size_t, int*);
using PaDrain = int (*)(void*, int*);
using PaFree = void (*)(void*);

struct Pulse {
  PaNew open = nullptr;
  PaWrite write = nullptr;
  PaDrain drain = nullptr;
  PaFree close = nullptr;
};

const Pulse* pulse() {
  static const Pulse p = [] {
    Pulse r;
    void* lib = dlopen("libpulse-simple.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!lib) return r;
    r.open = reinterpret_cast<PaNew>(dlsym(lib, "pa_simple_new"));
    r.write = reinterpret_cast<PaWrite>(dlsym(lib, "pa_simple_write"));
    r.drain = reinterpret_cast<PaDrain>(dlsym(lib, "pa_simple_drain"));
    r.close = reinterpret_cast<PaFree>(dlsym(lib, "pa_simple_free"));
    if (!r.open || !r.write || !r.drain || !r.close) r = Pulse{};
    return r;
  }();
  return p.open ? &p : nullptr;
}

bool playPulse(const std::vector<int16_t>& buf) {
  const Pulse* p = pulse();
  if (!p) return false;
  const PaSampleSpec spec{kPaSampleS16Le, kRate, 1};
  int err = 0;
  void* s = p->open(nullptr, "Monster for Cyberspace", kPaStreamPlayback, nullptr, "beep", &spec,
                    nullptr, nullptr, &err);
  if (!s) return false;
  p->write(s, buf.data(), buf.size() * sizeof(int16_t), &err);
  p->drain(s, &err);
  p->close(s);
  return true;
}

void playAlsa(const std::vector<int16_t>& buf) {
  const size_t frames = buf.size();
  snd_pcm_t* pcm = nullptr;
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) return;
  if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, kRate, 1,
                         50000) < 0) {
    snd_pcm_close(pcm);
    return;
  }
  for (size_t done = 0; done < frames;) {
    const snd_pcm_sframes_t n = snd_pcm_writei(pcm, buf.data() + done, frames - done);
    if (n < 0) {
      if (snd_pcm_recover(pcm, int(n), 1) < 0) break;
      continue;
    }
    done += size_t(n);
  }
  snd_pcm_drain(pcm);
  snd_pcm_close(pcm);
}

void play(int hz, int ms) {
  const int pct = muted ? 0 : appPct * keyPct / 100;
  if (pct <= 0 || hz <= 0 || ms <= 0) return;
  const size_t frames = size_t(kRate) * size_t(std::min(ms, kMaxMs)) / 1000;
  const int16_t amp = int16_t(kFullScale * pct / 100);
  const double period = double(kRate) / hz;
  std::vector<int16_t> buf(frames);
  for (size_t i = 0; i < frames; ++i) buf[i] = std::fmod(double(i), period) < period / 2 ? amp : -amp;
  // A few ms of fade at each end, so the amplifier doesn't click.
  const size_t ramp = std::min(frames / 4, size_t(kRate / 500));
  for (size_t i = 0; i < ramp; ++i) {
    buf[i] = int16_t(buf[i] * double(i) / ramp);
    buf[frames - 1 - i] = int16_t(buf[frames - 1 - i] * double(i) / ramp);
  }
  if (!playPulse(buf)) playAlsa(buf);
}

void loop() {
  std::unique_lock<std::mutex> lock(mu);
  for (;;) {
    cv.wait(lock, [] { return pending || !running; });
    if (!running) return;
    const int hz = nextHz, ms = nextMs;
    pending = false;
    lock.unlock();
    play(hz, ms);
    lock.lock();
  }
}

}  // namespace

void audioBegin() {
  std::lock_guard<std::mutex> lock(mu);
  if (running) return;
  running = true;
  worker = std::thread(loop);
}

void audioEnd() {
  {
    std::lock_guard<std::mutex> lock(mu);
    running = false;
  }
  cv.notify_all();
  if (worker.joinable()) worker.join();
}

void audioStepVolume(int deltaPct) {
  muted = false;
  keyPct = std::clamp(keyPct + deltaPct, 0, 100);
}

void audioToggleMute() { muted = !muted; }

}  // namespace cp0

namespace board {

// A beep asked for while one plays replaces any still waiting.
void beep(int freqHz, int ms) {
  {
    std::lock_guard<std::mutex> lock(cp0::mu);
    if (!cp0::running) return;
    cp0::nextHz = freqHz;
    cp0::nextMs = ms;
    cp0::pending = true;
  }
  cp0::cv.notify_one();
}

void setVolume(uint8_t pct) { cp0::appPct = std::min<int>(pct, 100); }

}  // namespace board

#endif
