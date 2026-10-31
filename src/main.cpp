#include <Arduino.h>
#include <esp_heap_caps.h>

#include "app/app_state.h"
#include "app/ota_service.h"
#include "app/time_service.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "core_version.h"
#include "hal/board.h"
#include "store/backup_store.h"
#include "store/nvs_migrate.h"
#include "store/settings.h"

using namespace rc;

namespace {
// Heap marks through setup, for boards without PSRAM.
void memMark(const char* step) {
  Serial.printf("[mem] %s: int %u blk %u\n", step, unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
}
}  // namespace

void setup() {
  // Large allocations (the message store, buffers) go to PSRAM.
  heap_caps_malloc_extmem_enable(64);
  Serial.begin(115200);
  Serial.printf("[boot] Monster for Cyberspace %s on %s\n", RC_VERSION, board::name());
  // Before anything reads NVS: settings saved under the old partition table.
  nvsmigrate::run();
  memMark("boot");
  board::begin();
  memMark("board");
  backupstore::begin();
  settings::load(app().cfg);
  timesvc::start(app().cfg);
  memMark("time");
  // The network source brings up Wi-Fi too; the demo build (RC_FAKE_API) has none.
  startChatSource(app().cfg);
  memMark("net");
  ui::start();
  ota::start();
  memMark("setup done");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
