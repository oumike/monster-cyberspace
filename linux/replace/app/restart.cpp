// SPDX-License-Identifier: GPL-3.0-or-later
// A restart is a fresh start of the process, as a reboot would give
// (ESP.restart() in linux/compat execs /proc/self/exe).
#include "app/restart.h"

#include <Arduino.h>

#include <chrono>
#include <thread>

namespace rc {

void restartSoon(uint32_t delayMs) {
  std::thread([delayMs] {
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));  // let any response go out
    ESP.restart();
  }).detach();
}

}  // namespace rc
