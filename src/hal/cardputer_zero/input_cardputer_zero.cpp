// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's keyboard: raw evdev from the kernel's TCA8418 driver,
// read on its own thread without a grab on the LCD (the launcher must still
// see a long Esc), through rc::CardputerZeroKeys (spec §2, §5). In a desktop
// window it is grabbed while the window has focus, so the desktop doesn't
// see the keys too (spec 2026-11-18 §4.4).
#if defined(BOARD_CARDPUTER_ZERO)

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "cp0_internal.h"
#include "hal/board.h"
#include "hal/linux_sim/desktop_mode.h"
#include "hal/linux_sim/sdl_window.h"
#include "input/keymap_cardputer_zero.h"

namespace cp0 {

namespace {

constexpr const char* kLauncherKeyboard = "/dev/input/by-path/platform-3f804000.i2c-event";
constexpr const char* kModifierState = "/sys/bus/i2c/devices/1-0034/modifier_state";
constexpr const char* kSymMapFile = "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map";

std::thread reader;
std::atomic<bool> running{false};
std::mutex keyMu;
std::deque<rc::ZeroKeyIn> keyQueue;
rc::CardputerZeroKeys keys;  // the UI thread only (pollKey)
std::atomic<bool> quit{false};
std::atomic<bool> focused{false};  // window mode: the window has keyboard focus

bool hasKeyA(int fd) {
  unsigned long bits[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof bits), bits) < 0) return false;
  const size_t w = 8 * sizeof(unsigned long);
  return (bits[KEY_A / w] >> (KEY_A % w)) & 1;
}

// MONSTER_KEYBOARD_DEVICE, APPLAUNCH_LINUX_KEYBOARD_DEVICE, the launcher's
// default, else the first event node with KEY_A. -1 if none opens.
int openKeyboard(std::string& path) {
  for (const char* env : {"MONSTER_KEYBOARD_DEVICE", "APPLAUNCH_LINUX_KEYBOARD_DEVICE"}) {
    if (const char* p = std::getenv(env); p && *p) {
      path = p;
      return ::open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
  }
  path = kLauncherKeyboard;
  if (const int fd = ::open(kLauncherKeyboard, O_RDONLY | O_NONBLOCK | O_CLOEXEC); fd >= 0) return fd;
  for (int i = 0; i < 32; ++i) {
    path = "/dev/input/event" + std::to_string(i);
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    if (hasKeyA(fd)) return fd;
    ::close(fd);
  }
  path.clear();
  return -1;
}

// modifier_state's shift and fn modes as kZeroMod* bits (0 inactive, 1 held,
// 2 one-shot, 3 locked); 0 when the file isn't there.
uint8_t modifierState() {
  FILE* f = std::fopen(kModifierState, "r");
  if (!f) return 0;
  uint8_t mods = 0;
  char line[64];
  while (std::fgets(line, sizeof line, f)) {
    char name[16] = {};
    int v = 0;
    if (std::sscanf(line, " %15[a-z_] %*[:=]%d", name, &v) != 2 && std::sscanf(line, " %15[a-z_] %d", name, &v) != 2)
      continue;
    if (v <= 0) continue;
    if (std::strcmp(name, "shift") == 0) mods |= rc::kZeroModShift;
    if (std::strcmp(name, "fn") == 0) mods |= rc::kZeroModFn;
  }
  std::fclose(f);
  return mods;
}

void loadSymMap() {
  FILE* f = std::fopen(kSymMapFile, "r");
  if (!f) {
    std::printf("[keys] no %s: the built-in sym table\n", kSymMapFile);
    return;
  }
  std::string text;
  char buf[1024];
  for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0 && text.size() < 64 * 1024;) text.append(buf, n);
  std::fclose(f);
  std::printf("[keys] sym table %s\n", keys.loadSymMap(text.c_str()) ? "from the system keymap" : "built in");
}

void readLoop() {
  int fd = -1;
  std::string path;
  bool told = false;
  bool grabbed = false;
  const bool window = linuxhal::windowed();
  while (running) {
    if (fd < 0) {
      fd = openKeyboard(path);
      if (fd < 0) {
        if (!told) std::fprintf(stderr, "[keys] no keyboard found; retrying\n");
        told = true;
        usleep(1000 * 1000);
        continue;
      }
      std::printf("[keys] %s\n", path.c_str());
      told = false;
      grabbed = false;
    }
    if (window && grabbed != focused) {
      if (::ioctl(fd, EVIOCGRAB, focused ? 1 : 0) == 0 || !focused) grabbed = focused;
    }
    pollfd p{fd, POLLIN, 0};
    const int r = ::poll(&p, 1, 100);
    if (r <= 0) continue;
    input_event ev[16];
    const ssize_t n = ::read(fd, ev, sizeof ev);
    if (n < 0) {
      if (errno == EAGAIN || errno == EINTR) continue;
      ::close(fd);  // unplugged or gone: look again
      fd = -1;
      continue;
    }
    for (size_t i = 0; i < size_t(n) / sizeof(input_event); ++i) {
      if (ev[i].type != EV_KEY || ev[i].value > 2) continue;
      if (window && !grabbed) continue;  // the desktop has these
      const uint8_t mods = ev[i].value == 1 ? modifierState() : 0;
      std::lock_guard<std::mutex> lock(keyMu);
      if (keyQueue.size() < 256)
        keyQueue.push_back({uint16_t(ev[i].code), uint8_t(ev[i].value), nowMs(), mods});
    }
  }
  if (fd >= 0) {
    if (grabbed) ::ioctl(fd, EVIOCGRAB, 0);
    ::close(fd);
  }
}

void systemKey(rc::ZeroSystemKey k) {
  // A monitor has its own brightness.
  if (linuxhal::windowed() &&
      (k == rc::ZeroSystemKey::BrightnessUp || k == rc::ZeroSystemKey::BrightnessDown))
    return;
  switch (k) {
    case rc::ZeroSystemKey::BrightnessUp:
      board::backlightLevel(uint8_t(std::min(255, currentBacklight() + 32)));
      break;
    case rc::ZeroSystemKey::BrightnessDown:
      board::backlightLevel(uint8_t(std::max(16, currentBacklight() - 32)));
      break;
    case rc::ZeroSystemKey::VolumeUp: audioStepVolume(10); break;
    case rc::ZeroSystemKey::VolumeDown: audioStepVolume(-10); break;
    case rc::ZeroSystemKey::Mute: audioToggleMute(); break;
    case rc::ZeroSystemKey::None: break;
  }
}

}  // namespace

void inputBegin() {
  loadSymMap();
  running = true;
  reader = std::thread(readLoop);
}

void inputEnd() {
  running = false;
  if (reader.joinable()) reader.join();
}

void inputFocus(bool f) { focused = f; }

void queueKey(uint16_t code, uint8_t value, uint32_t ms) {
  std::lock_guard<std::mutex> lock(keyMu);
  if (keyQueue.size() < 256) keyQueue.push_back({code, value, ms, 0});
}

bool escQuit() { return quit; }

}  // namespace cp0

namespace board {

bool pollKey(rc::KeyEvent& out) {
  using namespace cp0;
  if (linuxhal::windowed() && sdlwin::takeKey(out)) return true;
  for (;;) {
    rc::ZeroKeyIn in;
    {
      std::lock_guard<std::mutex> lock(keyMu);
      if (keyQueue.empty()) break;
      in = keyQueue.front();
      keyQueue.pop_front();
    }
    const bool got = keys.feed(in, out);
    systemKey(keys.takeSystemKey());
    if (keys.quitRequested()) quit = true;
    if (got) return true;
  }
  const bool got = keys.poll(nowMs(), out);
  if (keys.quitRequested()) quit = true;
  return got;
}

bool quitHold(uint32_t& heldMs, uint32_t& totalMs) {
  heldMs = cp0::keys.escHeldMs(cp0::nowMs());
  totalMs = rc::kZeroQuitHoldMs;
  return heldMs > 0;
}

}  // namespace board

#endif
