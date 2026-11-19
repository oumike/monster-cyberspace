// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(BOARD_CARDPUTER_ZERO) || defined(BOARD_LINUX_SIM)

#include "sdl_window.h"

#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include "ui/window_fit.h"

namespace sdlwin {

namespace {

namespace wf = rc::winfit;

constexpr uint32_t kResizeEveryMs = 50;
constexpr uint32_t kCursorIdleMs = 3000;

Options opts{};
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
int texW = 0, texH = 0;

const auto started = std::chrono::steady_clock::now();

// The main thread's view of the window: drawable pixels, and points (mouse
// events come in points; on a HiDPI screen the drawable is bigger).
std::atomic<int> drawW{1}, drawH{1}, winW{1}, winH{1};
std::atomic<int> zoomWanted{wf::kZoomDefault};
std::atomic<bool> minDirty{false};
std::atomic<uint8_t> level{255};
std::atomic<bool> redraw{true};

// The panel the UI thread draws at, and its last frame.
std::mutex frameMu;
int curW = 1, curH = 1, curZoom = 1;
std::vector<uint16_t> shown;
bool dirty = true;
uint32_t lastApplied = 0;  // UI thread

// The mouse, in drawable pixels. A press bumps pressSerial so even a click
// shorter than a UI frame is seen down once.
std::atomic<int> mouseX{0}, mouseY{0}, pressX{0}, pressY{0};
std::atomic<bool> leftDown{false};
std::atomic<uint32_t> pressSerial{0};
uint32_t seenSerial = 0;  // UI thread
bool pressOnPanel = false;

std::mutex keyMu;
std::deque<rc::KeyEvent> keyQueue;

// frameMu held.
wf::Fit currentFit() {
  wf::Fit f;
  f.panelW = curW;
  f.panelH = curH;
  f.zoom = curZoom;
  const int spareX = drawW - curW * curZoom, spareY = drawH - curH * curZoom;
  f.offX = spareX > 0 ? spareX / 2 : 0;
  f.offY = spareY > 0 ? spareY / 2 : 0;
  return f;
}

void refreshSize() {
  int w = 1, h = 1;
  SDL_GetRendererOutputSize(renderer, &w, &h);
  drawW = w > 0 ? w : 1;
  drawH = h > 0 ? h : 1;
  SDL_GetWindowSize(window, &w, &h);
  winW = w > 0 ? w : 1;
  winH = h > 0 ? h : 1;
  redraw = true;
}

void applyMinimum() {
  int w = 0, h = 0;
  wf::minWindow(opts.panelW, opts.panelH, zoomWanted, w, h);
  SDL_SetWindowMinimumSize(window, w, h);
}

// Window points to drawable pixels.
void toDrawable(int x, int y, int& dx, int& dy) {
  dx = int(int64_t(x) * drawW / winW);
  dy = int(int64_t(y) * drawH / winH);
}

void queue(rc::Action a, int times) {
  std::lock_guard<std::mutex> lock(keyMu);
  for (int i = 0; i < times && keyQueue.size() < 64; ++i) keyQueue.push_back(rc::KeyEvent{a});
}

void present() {
  SDL_Rect dst{};
  {
    std::lock_guard<std::mutex> lock(frameMu);
    if (!dirty && !redraw) return;
    if (!texture || texW != curW || texH != curH) {
      if (texture) SDL_DestroyTexture(texture);
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, curW, curH);
      texW = curW;
      texH = curH;
      dirty = true;
    }
    if (!texture) return;
    if (dirty) SDL_UpdateTexture(texture, nullptr, shown.data(), curW * 2);
    dirty = false;
    const wf::Fit f = currentFit();
    dst = {f.offX, f.offY, curW * curZoom, curH * curZoom};
  }
  redraw = false;
  const uint8_t v = level;
  SDL_SetTextureColorMod(texture, v, v, v);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, opts.resizable ? &dst : nullptr);
  SDL_RenderPresent(renderer);
}

// The last frame as a 24-bit BMP, for checks that can't see the window.
void writeShot(const char* path) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return;
  std::lock_guard<std::mutex> lock(frameMu);
  const uint32_t row = (uint32_t(curW) * 3 + 3) & ~3u;
  const uint32_t data = row * uint32_t(curH);
  uint8_t h[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) h[at + i] = uint8_t(v >> (8 * i));
  };
  put32(2, 54 + data);
  put32(10, 54);
  put32(14, 40);
  put32(18, uint32_t(curW));
  put32(22, uint32_t(curH));
  h[26] = 1;
  h[28] = 24;
  put32(34, data);
  std::fwrite(h, 1, sizeof h, f);
  std::vector<uint8_t> line(row, 0);
  for (int y = curH - 1; y >= 0; --y) {
    for (int x = 0; x < curW; ++x) {
      const uint16_t p = shown[size_t(y) * curW + x];
      line[x * 3 + 0] = uint8_t((p & 0x1f) << 3);
      line[x * 3 + 1] = uint8_t(((p >> 5) & 0x3f) << 2);
      line[x * 3 + 2] = uint8_t((p >> 11) << 3);
    }
    std::fwrite(line.data(), 1, line.size(), f);
  }
  std::fclose(f);
}

}  // namespace

uint32_t nowMs() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - started)
                      .count());
}

bool open(const Options& o) {
  opts = o;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
  // The desktop entry's StartupWMClass matches this, so the panel groups it.
  SDL_SetHint("SDL_VIDEO_WAYLAND_WMCLASS", "monster-cyberspace");
  SDL_SetHint("SDL_VIDEO_X11_WMCLASS", "monster-cyberspace");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "[window] SDL_Init: %s\n", SDL_GetError());
    return false;
  }
  const int w = o.resizable ? o.firstW : o.panelW * o.scale;
  const int h = o.resizable ? o.firstH : o.panelH * o.scale;
  const uint32_t flags = SDL_WINDOW_ALLOW_HIGHDPI | (o.resizable ? SDL_WINDOW_RESIZABLE : 0);
  window = SDL_CreateWindow(o.title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
  renderer = window ? SDL_CreateRenderer(window, -1, 0) : nullptr;
  if (!renderer) {
    std::fprintf(stderr, "[window] %s\n", SDL_GetError());
    if (window) SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
    return false;
  }
  refreshSize();
  std::lock_guard<std::mutex> lock(frameMu);
  if (o.resizable) {
    applyMinimum();
    const wf::Fit f = wf::fit(drawW, drawH, zoomWanted);
    curW = f.panelW;
    curH = f.panelH;
    curZoom = f.zoom;
  } else {
    SDL_RenderSetLogicalSize(renderer, o.panelW, o.panelH);
    curW = o.panelW;
    curH = o.panelH;
  }
  shown.assign(size_t(curW) * curH, 0);
  return true;
}

void run(bool (*quit)(), bool (*stop)(), void (*onKey)(const SDL_KeyboardEvent&),
         void (*onFocus)(bool)) {
  uint32_t lastMove = nowMs();
  bool cursorShown = true;
  auto moved = [&] {
    lastMove = nowMs();
    if (!cursorShown) SDL_ShowCursor(SDL_ENABLE);
    cursorShown = true;
  };
  while (!quit() && !(stop && stop())) {
    if (minDirty.exchange(false)) applyMinimum();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          return;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && onFocus) onFocus(true);
          if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && onFocus) onFocus(false);
          refreshSize();
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          if (opts.resizable && e.key.keysym.sym == SDLK_F11) {
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
              const bool full = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
              SDL_SetWindowFullscreen(window, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
          } else if (onKey) {
            onKey(e.key);
          }
          break;
        case SDL_MOUSEMOTION: {
          int x, y;
          toDrawable(e.motion.x, e.motion.y, x, y);
          mouseX = x;
          mouseY = y;
          moved();
          break;
        }
        case SDL_MOUSEBUTTONDOWN:
          moved();
          if (!opts.resizable) break;
          if (e.button.button == SDL_BUTTON_LEFT) {
            int x, y;
            toDrawable(e.button.x, e.button.y, x, y);
            mouseX = x;
            mouseY = y;
            pressX = x;
            pressY = y;
            leftDown = true;
            ++pressSerial;
          } else if (e.button.button == SDL_BUTTON_RIGHT) {
            queue(rc::Action::Options, 1);
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (e.button.button == SDL_BUTTON_LEFT) leftDown = false;
          break;
        case SDL_MOUSEWHEEL: {
          moved();
          if (!opts.resizable) break;
          int y = e.wheel.y;
          if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
          // SDL's y is positive away from the user; winfit's notches toward.
          const int steps = wf::wheelSteps(-y);
          if (steps) queue(steps < 0 ? rc::Action::Up : rc::Action::Down, steps < 0 ? -steps : steps);
          break;
        }
        default:
          break;
      }
    }
    if (opts.resizable && cursorShown && nowMs() - lastMove > kCursorIdleMs) {
      SDL_ShowCursor(SDL_DISABLE);
      cursorShown = false;
    }
    present();
    SDL_Delay(10);
  }
}

void close(const char* shotPath) {
  if (shotPath && *shotPath) writeShot(shotPath);
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  texture = nullptr;
  renderer = nullptr;
  window = nullptr;
  SDL_Quit();
}

int panelWidth() {
  std::lock_guard<std::mutex> lock(frameMu);
  return curW;
}

int panelHeight() {
  std::lock_guard<std::mutex> lock(frameMu);
  return curH;
}

void push(const uint16_t* frame, int y0, int y1) {
  if (!frame) return;
  std::lock_guard<std::mutex> lock(frameMu);
  if (y0 < 0) y0 = 0;
  if (y1 > curH) y1 = curH;
  if (y1 <= y0) return;
  std::memcpy(&shown[size_t(y0) * curW], frame + size_t(y0) * curW, size_t(y1 - y0) * curW * 2);
  dirty = true;
}

void setZoom(uint8_t zoom) {
  zoomWanted = wf::clampZoom(zoom);
  minDirty = true;
}

bool takePanelSize() {
  if (!opts.resizable) return false;
  const uint32_t now = nowMs();
  const wf::Fit f = wf::fit(drawW, drawH, zoomWanted);
  std::lock_guard<std::mutex> lock(frameMu);
  if (f.panelW == curW && f.panelH == curH && f.zoom == curZoom) return false;
  if (f.zoom == curZoom && now - lastApplied < kResizeEveryMs) return false;
  lastApplied = now;
  curW = f.panelW;
  curH = f.panelH;
  curZoom = f.zoom;
  shown.assign(size_t(curW) * curH, 0);
  dirty = true;
  return true;
}

bool readPointer(int16_t& x, int16_t& y) {
  if (!opts.resizable) return false;
  std::lock_guard<std::mutex> lock(frameMu);
  const wf::Fit f = currentFit();
  if (const uint32_t s = pressSerial; s != seenSerial) {
    seenSerial = s;
    pressOnPanel = wf::toPanel(f, pressX, pressY, x, y);
    return pressOnPanel;
  }
  if (!leftDown || !pressOnPanel) return false;
  wf::toPanel(f, mouseX, mouseY, x, y);
  return true;
}

bool takeKey(rc::KeyEvent& out) {
  std::lock_guard<std::mutex> lock(keyMu);
  if (keyQueue.empty()) return false;
  out = keyQueue.front();
  keyQueue.pop_front();
  return true;
}

void setLevel(uint8_t l) {
  level = l;
  redraw = true;
}

}  // namespace sdlwin

#endif
