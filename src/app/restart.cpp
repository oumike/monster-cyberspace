#include "app/restart.h"

#include <Arduino.h>

namespace rc {

namespace {

uint32_t gDelayMs = 0;

void restartTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(gDelayMs));  // let any response go out
  ESP.restart();
}

}  // namespace

void restartSoon(uint32_t delayMs) {
  gDelayMs = delayMs;
  xTaskCreate(restartTask, "restart", 2048, nullptr, 1, nullptr);
}

}  // namespace rc
