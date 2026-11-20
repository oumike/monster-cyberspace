// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/launcher.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <vector>

#include "ota/ota_logic.h"

namespace launcher {

namespace {

bool scan() {
  std::vector<rc::AppSlot> apps;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  for (; it; it = esp_partition_next(it)) {
    const esp_partition_t* p = esp_partition_get(it);
    apps.push_back(rc::AppSlot{p->address, static_cast<uint8_t>(p->subtype)});
  }
  esp_partition_iterator_release(it);
  const esp_partition_t* run = esp_ota_get_running_partition();
  const bool h = run && rc::launcherHosted(apps.data(), apps.size(), run->address);
  if (h) Serial.printf("[launcher] installed by Launcher, running at 0x%lx\n", (unsigned long)run->address);
  return h;
}

}  // namespace

bool hosted() {
  static const bool h = scan();
  return h;
}

}  // namespace launcher
