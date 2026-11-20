// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's display mode choice and single-instance lock
// (src/hal/linux_sim/desktop_mode, spec 2026-11-18 §3).
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "desktop_mode.h"

using linuxhal::chooseDisplayMode;
using linuxhal::DisplayMode;

int main() {
  // APPLaunch gives no display variables: the LCD.
  CHECK(chooseDisplayMode(nullptr, nullptr, nullptr) == DisplayMode::Lcd);
  CHECK(chooseDisplayMode(nullptr, "", "") == DisplayMode::Lcd);
  // The desktop gives one or both: a window.
  CHECK(chooseDisplayMode(nullptr, "wayland-1", nullptr) == DisplayMode::Window);
  CHECK(chooseDisplayMode(nullptr, nullptr, ":0") == DisplayMode::Window);
  CHECK(chooseDisplayMode(nullptr, "wayland-1", ":0") == DisplayMode::Window);
  // MONSTER_DISPLAY wins either way; anything else is ignored.
  CHECK(chooseDisplayMode("lcd", "wayland-1", ":0") == DisplayMode::Lcd);
  CHECK(chooseDisplayMode("window", nullptr, nullptr) == DisplayMode::Window);
  CHECK(chooseDisplayMode("tv", nullptr, nullptr) == DisplayMode::Lcd);
  CHECK(chooseDisplayMode("tv", "wayland-1", nullptr) == DisplayMode::Window);

  // The lock: the first taker keeps it; another process is refused until it
  // lets go.
  const std::string path = tempDir() + "/.lock";
  const int fd = linuxhal::lockInstance(path);
  CHECK(fd >= 0);
  auto otherProcessGets = [&] {
    const pid_t pid = fork();
    if (pid == 0) _exit(linuxhal::lockInstance(path) >= 0 ? 0 : 1);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
  };
  CHECK(!otherProcessGets());
  CHECK_EQ(linuxhal::lockInstance(path), linuxhal::kLockHeld);
  close(fd);
  CHECK(otherProcessGets());
  // A lock file that can't be made isn't "already running".
  CHECK_EQ(linuxhal::lockInstance("/nonexistent-dir/x/.lock"), linuxhal::kLockUnavailable);

  // The menu entry: copied into a directory made for it, rewritten when the
  // package's copy changes, and nothing when there is no package copy.
  const std::string src = tempDir() + "/entry.desktop";
  const std::string dst = tempDir() + "/share/applications/monster-cyberspace.desktop";
  auto write = [](const std::string& p, const std::string& s) { std::ofstream(p) << s; };
  auto read = [](const std::string& p) {
    std::ostringstream ss;
    ss << std::ifstream(p).rdbuf();
    return ss.str();
  };
  write(src, "[Desktop Entry]\nName=A\n");
  CHECK(linuxhal::installMenuEntry(src, dst));
  CHECK(read(dst) == "[Desktop Entry]\nName=A\n");
  CHECK(linuxhal::installMenuEntry(src, dst));
  write(src, "[Desktop Entry]\nName=B\n");
  CHECK(linuxhal::installMenuEntry(src, dst));
  CHECK(read(dst) == "[Desktop Entry]\nName=B\n");
  CHECK(!linuxhal::installMenuEntry(tempDir() + "/missing.desktop", tempDir() + "/out.desktop"));
  CHECK(access((tempDir() + "/out.desktop").c_str(), F_OK) != 0);
  return checkResult();
}
