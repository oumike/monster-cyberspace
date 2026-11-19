// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(BOARD_CARDPUTER_ZERO) || defined(BOARD_LINUX_SIM)

#include "desktop_mode.h"

#include <Preferences.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace linuxhal {

namespace {
bool set(const char* v) { return v && *v; }
}  // namespace

DisplayMode chooseDisplayMode(const char* force, const char* wayland, const char* x11) {
  if (set(force) && std::strcmp(force, "lcd") == 0) return DisplayMode::Lcd;
  if (set(force) && std::strcmp(force, "window") == 0) return DisplayMode::Window;
  return set(wayland) || set(x11) ? DisplayMode::Window : DisplayMode::Lcd;
}

int lockInstance(const std::string& path) {
  const int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if (fd < 0) return kLockUnavailable;
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    const bool held = errno == EWOULDBLOCK;
    ::close(fd);
    return held ? kLockHeld : kLockUnavailable;
  }
  return fd;
}

void lockOrExit() {
  const std::string dir = compat::configDir();
  // Without the directory there is nothing to share yet: run unlocked.
  if (!compat::makeDirs(dir)) return;
  // A lock file that can't be made or locked: run unlocked rather than not at all.
  if (lockInstance(dir + "/.lock") == kLockHeld) {
    std::printf("[boot] already running\n");
    std::fflush(stdout);
    std::_Exit(0);
  }
}

}  // namespace linuxhal

#endif
