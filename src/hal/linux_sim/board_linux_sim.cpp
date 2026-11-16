// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop simulator: the CardputerZero's 320x170 panel in an SDL2 window.
// The board's flags come from src/hal/cardputer_zero/flags_cardputer_zero.cpp.
#if defined(BOARD_LINUX_SIM)

#include <SDL.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include "hal/board.h"
#include "linux_hal.h"

namespace {

constexpr int kW = 320;
constexpr int kH = 170;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;

// pushRows runs on the UI thread, SDL on the main thread: frames meet here.
std::mutex frameMu;
std::vector<uint16_t> shown(kW * kH);
bool dirty = true;

std::atomic<bool> lit{true};
std::atomic<bool> asleep{false};
std::atomic<uint8_t> level{255};

int windowScale() {
  const char* s = std::getenv("MONSTER_SIM_SCALE");
  const int v = s ? std::atoi(s) : 3;
  return v >= 1 && v <= 8 ? v : 3;
}

void markDirty() {
  std::lock_guard<std::mutex> lock(frameMu);
  dirty = true;
}

void present() {
  {
    std::lock_guard<std::mutex> lock(frameMu);
    if (!dirty) return;
    SDL_UpdateTexture(texture, nullptr, shown.data(), kW * 2);
    dirty = false;
  }
  const uint8_t v = lit && !asleep ? level.load() : 0;
  SDL_SetTextureColorMod(texture, v, v, v);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

// MONSTER_SIM_SHOT=<file.bmp>: the last frame, written on exit (24-bit BMP),
// for checks that can't see the window.
void writeShot(const char* path) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return;
  const uint32_t row = kW * 3;  // 960: already a multiple of 4
  const uint32_t data = row * kH;
  const uint32_t fileSize = 54 + data;
  uint8_t h[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) h[at + i] = uint8_t(v >> (8 * i));
  };
  put32(2, fileSize);
  put32(10, 54);
  put32(14, 40);
  put32(18, kW);
  put32(22, kH);
  h[26] = 1;
  h[28] = 24;
  put32(34, data);
  std::fwrite(h, 1, sizeof h, f);
  std::vector<uint8_t> line(row);
  std::lock_guard<std::mutex> lock(frameMu);
  for (int y = kH - 1; y >= 0; --y) {
    for (int x = 0; x < kW; ++x) {
      const uint16_t p = shown[size_t(y) * kW + x];
      line[x * 3 + 0] = uint8_t((p & 0x1f) << 3);
      line[x * 3 + 1] = uint8_t(((p >> 5) & 0x3f) << 2);
      line[x * 3 + 2] = uint8_t((p >> 11) << 3);
    }
    std::fwrite(line.data(), 1, line.size(), f);
  }
  std::fclose(f);
}

}  // namespace

namespace board {

void begin() {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "[sim] SDL_Init: %s\n", SDL_GetError());
    std::exit(1);
  }
  const int s = windowScale();
  window = SDL_CreateWindow("Monster for Cyberspace (CardputerZero)", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, kW * s, kH * s, SDL_WINDOW_ALLOW_HIGHDPI);
  renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
  if (!renderer) {
    std::fprintf(stderr, "[sim] SDL window: %s\n", SDL_GetError());
    std::exit(1);
  }
  SDL_RenderSetLogicalSize(renderer, kW, kH);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kW, kH);
}

int panelWidth() { return kW; }
int panelHeight() { return kH; }

void pushRows(const uint16_t* frame, int y0, int y1) {
  if (!frame) return;
  if (y0 < 0) y0 = 0;
  if (y1 > kH) y1 = kH;
  if (y1 <= y0) return;
  std::lock_guard<std::mutex> lock(frameMu);
  std::memcpy(&shown[size_t(y0) * kW], frame + size_t(y0) * kW, size_t(y1 - y0) * kW * 2);
  dirty = true;
}

void backlight(bool on) {
  lit = on;
  markDirty();
}

void backlightLevel(uint8_t l) {
  level = l;
  lit = l > 0;
  markDirty();
}

void displaySleep(bool sleep) {
  asleep = sleep;
  markDirty();
}

bool pollKey(rc::KeyEvent&) { return false; }

int batteryPercent() { return 80; }
bool externalPower() { return false; }

void beep(int freqHz, int ms) {
  if (std::getenv("MONSTER_SIM_BEEP")) std::fprintf(stderr, "[sim] beep %d Hz %d ms\n", freqHz, ms);
}

void setVolume(uint8_t) {}

}  // namespace board

namespace linuxhal {

void runMainLoop(bool (*quit)()) {
  while (!quit()) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) return;
      if (e.type == SDL_WINDOWEVENT) markDirty();
    }
    present();
    SDL_Delay(10);
  }
}

void end() {
  if (const char* shot = std::getenv("MONSTER_SIM_SHOT")) writeShot(shot);
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  texture = nullptr;
  renderer = nullptr;
  window = nullptr;
  SDL_Quit();
}

}  // namespace linuxhal

#endif
