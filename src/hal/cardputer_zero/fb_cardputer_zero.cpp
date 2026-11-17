// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's ST7789v3 through the kernel's framebuffer (spec §5).
#if defined(BOARD_CARDPUTER_ZERO)

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "cp0_internal.h"

namespace cp0 {

namespace {

std::mutex mu;
int fd = -1;
void* base = nullptr;  // the mapping
uint8_t* mem = nullptr;  // the visible frame within it
size_t memLen = 0;
uint32_t lineLen = 0;  // bytes per framebuffer row
int fbW = 0, fbH = 0, bpp = 0;
fb_bitfield red{}, green{}, blue{};
std::vector<uint16_t> kept(size_t(kUiW) * kUiH);  // the last frame, for fbDark(false)
bool dark = false;

// The panel's node: $LV_LINUX_FBDEV_DEVICE, else the device's /dev/fb_lcd
// link, else the /proc/fb entry ("<n> <name>") named panel-mipi-dbid (the
// device's driver) or fb_st7789v (M5's docs), else /dev/fb0.
std::string panelNode() {
  if (const char* d = std::getenv("LV_LINUX_FBDEV_DEVICE"); d && *d) return d;
  if (access("/dev/fb_lcd", R_OK | W_OK) == 0) return "/dev/fb_lcd";
  if (FILE* f = std::fopen("/proc/fb", "r")) {
    int n;
    char name[64];
    while (std::fscanf(f, "%d %63s", &n, name) == 2) {
      if (std::strcmp(name, "panel-mipi-dbid") == 0 || std::strcmp(name, "fb_st7789v") == 0) {
        std::fclose(f);
        return "/dev/fb" + std::to_string(n);
      }
    }
    std::fclose(f);
  }
  return "/dev/fb0";
}

// An RGB565 pixel in the node's 32-bit layout (XRGB8888 normally).
uint32_t expand(uint16_t p) {
  const uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
  const uint32_t r8 = (r << 3) | (r >> 2), g8 = (g << 2) | (g >> 4), b8 = (b << 3) | (b >> 2);
  auto put = [](uint32_t v8, const fb_bitfield& f) {
    return f.length >= 8 ? v8 << (f.offset + f.length - 8) : (v8 >> (8 - f.length)) << f.offset;
  };
  return put(r8, red) | put(g8, green) | put(b8, blue);
}

void putPixel(uint8_t* row, int x, uint16_t p) {
  if (bpp == 16)
    reinterpret_cast<uint16_t*>(row)[x] = p;
  else
    reinterpret_cast<uint32_t*>(row)[x] = expand(p);
}

// Panel rows [y0, y1) from src (kUiW x kUiH, drawn kInsetX/kInsetY in, the
// margin repeating its edge pixels), or black when src is null.
void blit(const uint16_t* src, int y0, int y1) {
  const int w = std::min(kW, fbW);
  y0 = std::max(y0, 0);
  y1 = std::min(y1, fbH);
  for (int y = y0; y < y1; ++y) {
    uint8_t* row = mem + size_t(y) * lineLen;
    if (!src) {
      std::memset(row, 0, size_t(w) * size_t(bpp / 8));
      continue;
    }
    const uint16_t* in = src + size_t(std::clamp(y - kInsetY, 0, kUiH - 1)) * kUiW;
    for (int x = 0; x < w; ++x) putPixel(row, x, in[std::clamp(x - kInsetX, 0, kUiW - 1)]);
  }
}

// The panel rows a frame's rows [y0, y1) reach, the margin included at the ends.
void blitUi(const uint16_t* src, int y0, int y1) {
  blit(src, y0 == 0 ? 0 : y0 + kInsetY, y1 >= kUiH ? kH : y1 + kInsetY);
}

}  // namespace

bool fbOpen() {
  std::lock_guard<std::mutex> lock(mu);
  const std::string node = panelNode();
  fd = ::open(node.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    std::fprintf(stderr, "[fb] can't open %s\n", node.c_str());
    return false;
  }
  fb_var_screeninfo var{};
  fb_fix_screeninfo fix{};
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 ||
      (var.bits_per_pixel != 16 && var.bits_per_pixel != 32)) {
    std::fprintf(stderr, "[fb] %s: unsupported (%u bpp)\n", node.c_str(), var.bits_per_pixel);
    ::close(fd);
    fd = -1;
    return false;
  }
  fbW = int(var.xres);
  fbH = int(var.yres);
  bpp = int(var.bits_per_pixel);
  red = var.red;
  green = var.green;
  blue = var.blue;
  lineLen = fix.line_length ? fix.line_length : uint32_t(fbW * bpp / 8);
  memLen = fix.smem_len ? fix.smem_len : size_t(lineLen) * fbH;
  void* m = mmap(nullptr, memLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (m == MAP_FAILED) {
    std::fprintf(stderr, "[fb] %s: mmap failed\n", node.c_str());
    ::close(fd);
    fd = -1;
    return false;
  }
  base = m;
  mem = static_cast<uint8_t*>(m) + size_t(var.yoffset) * lineLen + size_t(var.xoffset) * (bpp / 8);
  std::printf("[fb] %s %dx%d %d bpp\n", node.c_str(), fbW, fbH, bpp);
  if (fbW != kW || fbH != kH) std::printf("[fb] expected %dx%d; drawing the top-left corner\n", kW, kH);
  blit(nullptr, 0, fbH);
  return true;
}

void fbPush(const uint16_t* frame, int y0, int y1) {
  if (!frame) return;
  y0 = std::max(y0, 0);
  y1 = std::min(y1, kUiH);
  if (y1 <= y0) return;
  std::lock_guard<std::mutex> lock(mu);
  std::memcpy(&kept[size_t(y0) * kUiW], frame + size_t(y0) * kUiW, size_t(y1 - y0) * kUiW * 2);
  if (mem && !dark) blitUi(frame, y0, y1);
}

void fbDark(bool on) {
  std::lock_guard<std::mutex> lock(mu);
  if (dark == on) return;
  dark = on;
  if (!mem) return;
  if (on)
    blit(nullptr, 0, fbH);
  else
    blitUi(kept.data(), 0, kUiH);
}

void fbClose() {
  std::lock_guard<std::mutex> lock(mu);
  if (!mem) return;
  blit(nullptr, 0, fbH);
  munmap(base, memLen);
  ::close(fd);
  base = nullptr;
  mem = nullptr;
  fd = -1;
}

}  // namespace cp0

#endif
