#include <Arduino.h>
#include <esp_heap_caps.h>

#include "app/app_state.h"
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "core_version.h"
#include "hal/board.h"
#include "store/backup_store.h"
#include "store/settings.h"

using namespace rc;

void setup() {
  // Large allocations (the message store, buffers) go to PSRAM.
  heap_caps_malloc_extmem_enable(64);
  Serial.begin(115200);
  Serial.printf("[boot] platy-cyberspace %s on %s\n", RC_VERSION, board::name());
  board::begin();
  backupstore::begin();
  settings::load(app().cfg);
  timesvc::start(app().cfg);
  // The network source brings up Wi-Fi too.
  startChatSource(app().cfg);
  ui::start();
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
