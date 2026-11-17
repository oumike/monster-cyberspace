// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's battery (BQ27220) and backlight through sysfs (spec §5).
#if defined(BOARD_CARDPUTER_ZERO)

#include <dirent.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "cp0_internal.h"
#include "hal/board.h"

namespace cp0 {

namespace {

std::string batteryDir;    // /sys/class/power_supply/<battery>, or empty
std::string backlightDir;  // /sys/class/backlight/<first>, or empty
int maxBrightness = 0;
int launcherBrightness = -1;  // what it was before the app, put back on exit

bool readLine(const std::string& path, std::string& out) {
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  char buf[64] = {};
  const bool ok = std::fgets(buf, sizeof buf, f) != nullptr;
  std::fclose(f);
  if (!ok) return false;
  out = buf;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
  return true;
}

int readInt(const std::string& path) {
  std::string s;
  return readLine(path, s) ? std::atoi(s.c_str()) : -1;
}

bool writeInt(const std::string& path, int v) {
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  const bool ok = std::fprintf(f, "%d\n", v) > 0;
  return std::fclose(f) == 0 && ok;
}

// The first entry of dir that pick accepts, as a full path.
template <typename Pick>
std::string firstEntry(const char* dir, Pick pick) {
  std::string found;
  if (DIR* d = opendir(dir)) {
    while (dirent* e = readdir(d)) {
      if (e->d_name[0] == '.') continue;
      const std::string p = std::string(dir) + "/" + e->d_name;
      if (pick(p)) {
        found = p;
        break;
      }
    }
    closedir(d);
  }
  return found;
}

// Battery reads are cached for 5 s: the status bar asks often.
std::mutex batteryMu;
uint32_t batteryAt = 0;
bool batteryRead = false;
std::atomic<int> percent{-1};
std::atomic<bool> charging{false};

void refreshBattery() {
  std::lock_guard<std::mutex> lock(batteryMu);
  const uint32_t now = nowMs();
  if (batteryRead && now - batteryAt < 5000) return;
  batteryRead = true;
  batteryAt = now;
  if (batteryDir.empty()) return;
  percent = readInt(batteryDir + "/capacity");
  if (percent > 100) percent = 100;
  std::string status;
  charging = readLine(batteryDir + "/status", status) && (status == "Charging" || status == "Full");
}

}  // namespace

void powerBegin() {
  batteryDir = firstEntry("/sys/class/power_supply", [](const std::string& p) {
    std::string type;
    return readLine(p + "/type", type) && type == "Battery";
  });
  backlightDir = firstEntry("/sys/class/backlight", [](const std::string& p) {
    return readInt(p + "/max_brightness") > 0;
  });
  if (!backlightDir.empty()) {
    maxBrightness = readInt(backlightDir + "/max_brightness");
    launcherBrightness = readInt(backlightDir + "/brightness");
  }
  std::printf("[power] battery %s, backlight %s\n", batteryDir.empty() ? "none" : batteryDir.c_str(),
              backlightDir.empty() ? "none" : backlightDir.c_str());
}

bool hasBacklightNode() { return !backlightDir.empty(); }

void backlightWrite(uint8_t level) {
  if (backlightDir.empty()) return;
  int v = (int(level) * maxBrightness + 127) / 255;
  if (level > 0 && v == 0) v = 1;
  writeInt(backlightDir + "/brightness", v);
}

void powerEnd() {
  if (!backlightDir.empty() && launcherBrightness >= 0)
    writeInt(backlightDir + "/brightness", launcherBrightness);
}

}  // namespace cp0

namespace board {

int batteryPercent() {
  cp0::refreshBattery();
  return cp0::percent;
}

bool externalPower() {
  cp0::refreshBattery();
  return cp0::charging;
}

}  // namespace board

#endif
