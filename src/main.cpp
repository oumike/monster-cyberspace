#include <Arduino.h>
#include <esp_heap_caps.h>

#include "core_version.h"

void setup() {
  // Chat data is many small strings: send everything over 64 bytes to PSRAM.
  heap_caps_malloc_extmem_enable(64);
  Serial.begin(115200);
  Serial.printf("[boot] rhino-cyberspace %s board=M9\n", RC_VERSION);
}

void loop() { delay(1000); }
