// SPDX-License-Identifier: GPL-3.0-or-later
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <pthread.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

uint32_t millis();

// ---- tasks ----

struct compat_task {
  std::string name;
};

namespace {

std::mutex tasksLock;
std::multimap<std::string, compat_task*> tasks;

void nameThread(const char* name) {
  char n[16];
  std::snprintf(n, sizeof n, "%s", name);
#if defined(__APPLE__)
  pthread_setname_np(n);
#else
  pthread_setname_np(pthread_self(), n);
#endif
}

}  // namespace

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t, void* arg, UBaseType_t,
                                   TaskHandle_t* out, BaseType_t) {
  auto* t = new compat_task{name ? name : ""};
  {
    std::lock_guard<std::mutex> g(tasksLock);
    tasks.emplace(t->name, t);
  }
  if (out) *out = t;
  std::thread([fn, arg, t] {
    nameThread(t->name.c_str());
    fn(arg);
    // A FreeRTOS task must not return; here returning ends it.
    std::lock_guard<std::mutex> g(tasksLock);
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
      if (it->second == t) {
        tasks.erase(it);
        break;
      }
    }
    // The handle stays allocated: a caller may still hold it.
  }).detach();
  return pdPASS;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stack, void* arg, UBaseType_t prio,
                       TaskHandle_t* out) {
  return xTaskCreatePinnedToCore(fn, name, stack, arg, prio, out, 0);
}

void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }

TickType_t xTaskGetTickCount() { return millis(); }

TaskHandle_t xTaskGetHandle(const char* name) {
  std::lock_guard<std::mutex> g(tasksLock);
  auto it = tasks.find(name ? name : "");
  return it == tasks.end() ? nullptr : it->second;
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 64 * 1024; }

// ---- mutexes ----

struct compat_mutex {
  std::timed_mutex m;
};

SemaphoreHandle_t xSemaphoreCreateMutex() { return new compat_mutex; }

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
  if (!s) return pdFALSE;
  if (ticks == portMAX_DELAY) {
    s->m.lock();
    return pdTRUE;
  }
  return s->m.try_lock_for(std::chrono::milliseconds(ticks)) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
  if (!s) return pdFALSE;
  s->m.unlock();
  return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }

// ---- esp_timer ----

struct esp_timer {
  esp_timer_create_args_t args;
  std::mutex m;
  std::condition_variable cv;
  uint64_t gen = 0;  // bumped by each start and stop, to end the old thread
  bool deleted = false;
};

esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out) {
  if (!args || !args->callback || !out) return ESP_ERR_INVALID_ARG;
  auto* t = new esp_timer;
  t->args = *args;
  *out = t;
  return ESP_OK;
}

namespace {

esp_err_t start(esp_timer_handle_t t, uint64_t us, bool periodic) {
  if (!t) return ESP_ERR_INVALID_ARG;
  uint64_t gen;
  {
    std::lock_guard<std::mutex> g(t->m);
    if (t->deleted) return ESP_ERR_INVALID_STATE;
    gen = ++t->gen;
  }
  t->cv.notify_all();
  std::thread([t, us, periodic, gen] {
    if (t->args.name) nameThread(t->args.name);
    auto next = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(t->m);
    for (;;) {
      next += std::chrono::microseconds(us);
      if (t->cv.wait_until(lk, next, [&] { return t->gen != gen; })) return;
      lk.unlock();
      t->args.callback(t->args.arg);
      lk.lock();
      if (!periodic || t->gen != gen) return;
    }
  }).detach();
  return ESP_OK;
}

}  // namespace

esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) { return start(t, us, false); }
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) { return start(t, us, true); }

esp_err_t esp_timer_stop(esp_timer_handle_t t) {
  if (!t) return ESP_ERR_INVALID_ARG;
  {
    std::lock_guard<std::mutex> g(t->m);
    ++t->gen;
  }
  t->cv.notify_all();
  return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t t) {
  if (!t) return ESP_ERR_INVALID_ARG;
  std::lock_guard<std::mutex> g(t->m);
  ++t->gen;
  t->deleted = true;
  t->cv.notify_all();
  // Not freed: a timer thread may still be waking up to see the stop.
  return ESP_OK;
}
