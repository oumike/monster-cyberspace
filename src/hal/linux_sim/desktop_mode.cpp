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
#include <fstream>
#include <sstream>

namespace linuxhal {

namespace {
bool set(const char* v) { return v && *v; }

bool readAll(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

constexpr const char* kPackageEntry = "/usr/share/monster-cyberspace/monster-cyberspace.desktop";
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

bool installMenuEntry(const std::string& src, const std::string& dst) {
  std::string want, have;
  if (!readAll(src, want)) return false;
  if (readAll(dst, have) && have == want) return true;
  const size_t slash = dst.rfind('/');
  if (slash != std::string::npos && !compat::makeDirs(dst.substr(0, slash))) return false;
  const std::string tmp = dst + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out || !(out << want) || !out.flush()) return false;
  }
  return std::rename(tmp.c_str(), dst.c_str()) == 0;
}

void installMenuEntryForUser() {
  std::string data;
  if (set(std::getenv("XDG_DATA_HOME"))) data = std::getenv("XDG_DATA_HOME");
  else if (set(std::getenv("HOME"))) data = std::string(std::getenv("HOME")) + "/.local/share";
  else return;
  // False when run from a build rather than the package: nothing to copy.
  installMenuEntry(kPackageEntry, data + "/applications/monster-cyberspace.desktop");
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
