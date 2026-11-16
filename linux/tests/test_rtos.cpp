// SPDX-License-Identifier: GPL-3.0-or-later
// The Arduino, FreeRTOS, esp_timer and heap_caps shims.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "check.h"

namespace {

void clocks() {
  const uint32_t m0 = millis();
  const int64_t u0 = esp_timer_get_time();
  delay(30);
  CHECK(millis() - m0 >= 30);
  CHECK(esp_timer_get_time() - u0 >= 30000);
  vTaskDelay(pdMS_TO_TICKS(20));
  CHECK(millis() - m0 >= 50);
}

void randoms() {
  for (int i = 0; i < 200; ++i) {
    const long a = random(0, 2);
    CHECK(a == 0 || a == 1);
    const long b = random(5);
    CHECK(b >= 0 && b < 5);
  }
  bool differ = false;
  const uint32_t first = esp_random();
  for (int i = 0; i < 8; ++i) differ |= esp_random() != first;
  CHECK(differ);
}

void mutexes() {
  SemaphoreHandle_t m = xSemaphoreCreateMutex();
  CHECK(m != nullptr);
  CHECK(xSemaphoreTake(m, portMAX_DELAY) == pdTRUE);
  std::atomic<int> got{-1};
  std::thread other([&] {
    const auto t0 = std::chrono::steady_clock::now();
    got = xSemaphoreTake(m, pdMS_TO_TICKS(50));
    CHECK(std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(45));
  });
  other.join();
  CHECK(got == pdFALSE);
  CHECK(xSemaphoreGive(m) == pdTRUE);
  std::thread again([&] {
    got = xSemaphoreTake(m, pdMS_TO_TICKS(50));
    if (got == pdTRUE) xSemaphoreGive(m);
  });
  again.join();
  CHECK(got == pdTRUE);
}

std::atomic<bool> release{false};
std::atomic<int> ran{0};

void taskBody(void* arg) {
  ran = *static_cast<int*>(arg);
  while (!release) delay(1);
}

void tasks() {
  static int arg = 7;
  TaskHandle_t h = nullptr;
  CHECK(xTaskCreatePinnedToCore(taskBody, "t1", 4096, &arg, 1, &h, 1) == pdPASS);
  CHECK(h != nullptr);
  for (int i = 0; i < 200 && ran != 7; ++i) delay(1);
  CHECK(ran == 7);
  CHECK(xTaskGetHandle("t1") == h);
  CHECK(xTaskGetHandle("nope") == nullptr);
  CHECK(uxTaskGetStackHighWaterMark(nullptr) > 0);
  release = true;
  for (int i = 0; i < 200 && xTaskGetHandle("t1"); ++i) delay(1);
  CHECK(xTaskGetHandle("t1") == nullptr);  // gone once its function returned
}

std::atomic<int> ticks{0};

void periodicTimer() {
  esp_timer_create_args_t args = {};
  args.callback = [](void* a) { ++*static_cast<std::atomic<int>*>(a); };
  args.arg = &ticks;
  args.name = "t";
  esp_timer_handle_t t = nullptr;
  CHECK(esp_timer_create(&args, &t) == ESP_OK);
  CHECK(esp_timer_start_periodic(t, 20000) == ESP_OK);
  for (int i = 0; i < 2000 && ticks < 3; ++i) delay(1);
  CHECK(ticks >= 3);
  CHECK(esp_timer_stop(t) == ESP_OK);
  delay(5);
  const int n = ticks;
  delay(60);
  CHECK_EQ(int(ticks), n);  // stopped
  CHECK(esp_timer_delete(t) == ESP_OK);
}

void heap() {
  // Linux has room to spare: the low-memory paths never trigger.
  CHECK(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= 1u << 20);
  CHECK(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= 1u << 20);
  CHECK(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) >= 1u << 20);
  CHECK(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= 1u << 20);
}

void strings() {
  String s("abc");
  CHECK(std::string(s.c_str()) == "abc");
  CHECK_EQ(s.length(), size_t(3));
}

}  // namespace

int main() {
  clocks();
  randoms();
  mutexes();
  tasks();
  periodicTimer();
  heap();
  strings();
  return checkResult();
}
