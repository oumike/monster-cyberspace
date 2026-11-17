// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop simulator: the CardputerZero's 320x170 panel in an SDL2 window.
// The board's flags come from src/hal/cardputer_zero/flags_cardputer_zero.cpp.
#if defined(BOARD_LINUX_SIM)

#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "hal/board.h"
#include "input/keymap_cardputer_zero.h"
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

// SDL key events (main thread) become the codes the device's kernel sends
// (spec section 5.1) and wait here for pollKey (UI thread).
std::mutex keyMu;
std::deque<rc::ZeroKeyIn> keyQueue;
std::unordered_map<int, uint16_t> downCodes;  // SDL scancode -> code sent on press
rc::CardputerZeroKeys keys;                   // UI thread only
std::atomic<bool> escQuit{false};
const auto started = std::chrono::steady_clock::now();

uint32_t nowMs() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - started)
                      .count());
}

// The sym-layer code that types c, from the launcher's built-in table.
uint16_t symCode(char c) {
  static constexpr struct {
    char c;
    uint16_t code;
  } kSym[] = {
      {'`', 69}, {'~', 55}, {'[', 74}, {'{', 76}, {']', 75}, {'}', 77}, {'\\', 86}, {'|', 89},
      {';', 79}, {':', 80}, {'\'', 81}, {'"', 82}, {',', 90}, {'<', 83}, {'.', 91}, {'>', 85},
      {'/', 92}, {'?', 93},
  };
  for (const auto& k : kSym) {
    if (k.c == c) return k.code;
  }
  return 0;
}

// The evdev code for a host key (US layout), or 0.
uint16_t evdevCode(SDL_Keycode k, bool shift) {
  using namespace rc::zerokey;
  if (k >= SDLK_a && k <= SDLK_z) {
    static constexpr uint16_t kLetters[] = {kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM,
                                            kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ};
    return kLetters[k - SDLK_a];
  }
  if (k >= SDLK_1 && k <= SDLK_9) return uint16_t(k1 + (k - SDLK_1));
  if (k >= SDLK_F1 && k <= SDLK_F10) return uint16_t(kF1 + (k - SDLK_F1));
  switch (k) {
    case SDLK_0: return k0;
    case SDLK_MINUS: return kMinus;
    case SDLK_EQUALS: return kEqual;
    case SDLK_SPACE: return kSpace;
    case SDLK_ESCAPE: return kEsc;
    case SDLK_RETURN: return kEnter;
    case SDLK_KP_ENTER: return kKpEnter;
    case SDLK_TAB: return kTab;
    case SDLK_BACKSPACE: return kBackspace;
    case SDLK_DELETE: return kDelete;
    case SDLK_UP: return kUp;
    case SDLK_DOWN: return kDown;
    case SDLK_LEFT: return kLeft;
    case SDLK_RIGHT: return kRight;
    case SDLK_HOME: return kHome;
    case SDLK_END: return kEnd;
    case SDLK_PAGEUP: return kPageUp;
    case SDLK_PAGEDOWN: return kPageDown;
    case SDLK_F11: return kF11;
    case SDLK_F12: return kF12;
    case SDLK_LSHIFT: return kLeftShift;
    case SDLK_RSHIFT: return kRightShift;
    case SDLK_LALT: return kFn;
    case SDLK_RALT: return kCompose;
    case SDLK_BACKQUOTE: return symCode(shift ? '~' : '`');
    case SDLK_LEFTBRACKET: return symCode(shift ? '{' : '[');
    case SDLK_RIGHTBRACKET: return symCode(shift ? '}' : ']');
    case SDLK_BACKSLASH: return symCode(shift ? '|' : '\\');
    case SDLK_SEMICOLON: return symCode(shift ? ':' : ';');
    case SDLK_QUOTE: return symCode(shift ? '"' : '\'');
    case SDLK_COMMA: return symCode(shift ? '<' : ',');
    case SDLK_PERIOD: return symCode(shift ? '>' : '.');
    case SDLK_SLASH: return symCode(shift ? '?' : '/');
    default: return 0;
  }
}

void queueKey(const SDL_KeyboardEvent& k) {
  if (k.repeat) return;  // the keymap times its own repeats
  uint16_t code = 0;
  if (k.type == SDL_KEYDOWN) {
    code = evdevCode(k.keysym.sym, (k.keysym.mod & KMOD_SHIFT) != 0);
    if (!code) return;
    downCodes[k.keysym.scancode] = code;
  } else {
    const auto it = downCodes.find(k.keysym.scancode);
    if (it == downCodes.end()) return;
    code = it->second;
    downCodes.erase(it);
  }
  std::lock_guard<std::mutex> lock(keyMu);
  keyQueue.push_back({code, uint8_t(k.type == SDL_KEYDOWN ? 1 : 0), nowMs(), 0});
}

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

bool pollKey(rc::KeyEvent& out) {
  for (;;) {
    rc::ZeroKeyIn in;
    {
      std::lock_guard<std::mutex> lock(keyMu);
      if (keyQueue.empty()) break;
      in = keyQueue.front();
      keyQueue.pop_front();
    }
    const bool got = keys.feed(in, out);
    keys.takeSystemKey();  // no backlight or volume keys on the host
    if (keys.quitRequested()) escQuit = true;
    if (got) return true;
  }
  const bool got = keys.poll(nowMs(), out);
  if (keys.quitRequested()) escQuit = true;
  return got;
}

int batteryPercent() { return 80; }
bool externalPower() { return false; }

void beep(int freqHz, int ms) {
  if (std::getenv("MONSTER_SIM_BEEP")) std::fprintf(stderr, "[sim] beep %d Hz %d ms\n", freqHz, ms);
}

void setVolume(uint8_t) {}

}  // namespace board

namespace linuxhal {

void runMainLoop(bool (*quit)()) {
  while (!quit() && !escQuit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) return;
      if (e.type == SDL_WINDOWEVENT) markDirty();
      if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) queueKey(e.key);
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
